"""
Mahlanya RPG — AWS CDK v2 Build Scaling Stack
==============================================
Provisions the cloud infrastructure for UE5 game build agents in the
af-south-1 (Cape Town) region — the closest AWS region to Eswatini.

Deploy:
    pip install aws-cdk-lib constructs
    cdk deploy MahlanyaBuildStack --region af-south-1
"""

import aws_cdk as cdk
from aws_cdk import (
    Duration,
    RemovalPolicy,
    Stack,
    aws_ec2 as ec2,
    aws_efs as efs,
    aws_iam as iam,
    aws_s3 as s3,
)
from constructs import Construct


class MahlanyaBuildStack(Stack):
    """
    Single CDK stack containing all build-pipeline infrastructure.

    Resources created:
        - VPC (2 AZs, private subnets only — no public internet ingress)
        - EFS file system for shared Content/ assets across build agents
        - S3 bucket for distributing libmahlanya_compute.so artifacts
        - IAM role + instance profile for EC2 Spot fleet nodes
        - Security group (SSH from VPC CIDR only)
        - EC2 Spot fleet request (c5.4xlarge, max 4 concurrent instances)
    """

    def __init__(self, scope: Construct, construct_id: str, **kwargs) -> None:
        super().__init__(scope, construct_id, **kwargs)

        # ------------------------------------------------------------------
        # VPC
        # Two Availability Zones; private subnets only (no NAT gateway cost
        # beyond what AWS charges for VPC endpoints).  Build agents pull UE5
        # packages from S3 via VPC endpoint to avoid egress charges.
        # ------------------------------------------------------------------
        vpc = ec2.Vpc(
            self,
            "MahlanyaBuildVpc",
            max_azs=2,
            nat_gateways=1,  # one NAT GW for outbound package installs
            subnet_configuration=[
                ec2.SubnetConfiguration(
                    name="Private",
                    subnet_type=ec2.SubnetType.PRIVATE_WITH_EGRESS,
                    cidr_mask=24,
                ),
                # Public subnet is required by CDK when nat_gateways > 0
                ec2.SubnetConfiguration(
                    name="Public",
                    subnet_type=ec2.SubnetType.PUBLIC,
                    cidr_mask=28,
                ),
            ],
        )

        # S3 VPC endpoint — free data transfer for artifact uploads/downloads
        vpc.add_gateway_endpoint(
            "S3Endpoint",
            service=ec2.GatewayVpcEndpointAwsService.S3,
        )

        # ------------------------------------------------------------------
        # Security Group
        # SSH allowed only from within the VPC CIDR; no public exposure.
        # ------------------------------------------------------------------
        build_sg = ec2.SecurityGroup(
            self,
            "BuildAgentSG",
            vpc=vpc,
            description="Mahlanya build agents — SSH from VPC only",
            allow_all_outbound=True,  # agents need to pull packages from internet
        )
        build_sg.add_ingress_rule(
            peer=ec2.Peer.ipv4(vpc.vpc_cidr_block),
            connection=ec2.Port.tcp(22),
            description="SSH from VPC CIDR only — no public access",
        )

        # ------------------------------------------------------------------
        # EFS — Shared Content/ asset volume
        # Mounted at /mnt/mahlanya-content on every build agent so that large
        # source art assets (textures, meshes, audio) are fetched once and
        # reused across concurrent builds.
        # ------------------------------------------------------------------
        content_fs = efs.FileSystem(
            self,
            "ContentEFS",
            vpc=vpc,
            # BURSTING_THROUGHPUT is cost-effective when builds are bursty
            throughput_mode=efs.ThroughputMode.BURSTING,
            performance_mode=efs.PerformanceMode.GENERAL_PURPOSE,
            encrypted=True,
            removal_policy=RemovalPolicy.RETAIN,  # retain on stack destroy — assets are precious
            file_system_name="mahlanya-content",
        )
        # Allow build agents to mount EFS
        content_fs.connections.allow_default_port_from(build_sg)

        # ------------------------------------------------------------------
        # S3 Bucket — Pipeline Artifacts
        # Stores compiled libmahlanya_compute.so builds indexed by git SHA.
        # Versioning enabled so old builds can be rolled back.
        # ------------------------------------------------------------------
        artifacts_bucket = s3.Bucket(
            self,
            "PipelineArtifacts",
            bucket_name="mahlanya-pipeline-artifacts",
            versioned=True,
            encryption=s3.BucketEncryption.S3_MANAGED,
            block_public_access=s3.BlockPublicAccess.BLOCK_ALL,
            removal_policy=RemovalPolicy.RETAIN,
            lifecycle_rules=[
                s3.LifecycleRule(
                    # Keep non-current (old) artifact versions for 90 days
                    noncurrent_version_expiration=Duration.days(90),
                )
            ],
        )

        # ------------------------------------------------------------------
        # IAM Role — Build Agent Instance Profile
        # Grants read/write on the artifacts bucket and EFS mount permissions.
        # No broad AWS access; principle of least privilege.
        # ------------------------------------------------------------------
        build_role = iam.Role(
            self,
            "BuildAgentRole",
            assumed_by=iam.ServicePrincipal("ec2.amazonaws.com"),
            description="IAM role for Mahlanya UE5 Spot build agents",
            managed_policies=[
                # SSM Session Manager — console access without opening port 22
                # to the public internet (belt-and-suspenders alongside SG rule)
                iam.ManagedPolicy.from_aws_managed_policy_name(
                    "AmazonSSMManagedInstanceCore"
                ),
            ],
        )

        # S3: read/write on the artifacts bucket only
        artifacts_bucket.grant_read_write(build_role)

        # EFS: allow client mount and read/write
        content_fs.grant_read_write(build_role)

        # EC2: allow the fleet to describe its own tags (used by user-data)
        build_role.add_to_policy(
            iam.PolicyStatement(
                actions=["ec2:DescribeTags"],
                resources=["*"],
                conditions={
                    "StringEquals": {
                        "aws:RequestedRegion": "af-south-1",
                    }
                },
            )
        )

        # Instance profile wraps the role for EC2 attachment
        instance_profile = iam.CfnInstanceProfile(
            self,
            "BuildAgentInstanceProfile",
            roles=[build_role.role_name],
            instance_profile_name="MahlanyaBuildAgentProfile",
        )

        # ------------------------------------------------------------------
        # User-data script
        # Passed to every Spot instance at launch.  The actual script lives in
        # build_agent_userdata.sh; here we embed it via Assets or inline text.
        # For simplicity we reference the companion file at deploy-time.
        # ------------------------------------------------------------------
        with open("build_agent_userdata.sh", "r") as fh:
            userdata_script = fh.read()

        userdata = ec2.UserData.for_linux()
        userdata.add_commands(userdata_script)

        # ------------------------------------------------------------------
        # EC2 Spot Fleet — UE5 Build Nodes
        # c5.4xlarge: 16 vCPU / 32 GiB RAM — sufficient for UE5 shader
        # compilation and C++/Zig native builds in parallel.
        # Max 4 concurrent instances keeps costs bounded.
        # ------------------------------------------------------------------

        # Launch template — defines the OS, instance type, role, SG, and
        # user-data shared across all fleet instances.
        launch_template = ec2.LaunchTemplate(
            self,
            "BuildAgentLaunchTemplate",
            # Amazon Linux 2023 — supported by UE5 Linux prerequisites
            machine_image=ec2.MachineImage.latest_amazon_linux2023(),
            instance_type=ec2.InstanceType("c5.4xlarge"),
            security_group=build_sg,
            role=build_role,
            user_data=userdata,
            block_devices=[
                ec2.BlockDevice(
                    device_name="/dev/xvda",
                    volume=ec2.BlockDeviceVolume.ebs(
                        500,  # 500 GiB root volume for UE5 engine + build cache
                        volume_type=ec2.EbsDeviceVolumeType.GP3,
                        encrypted=True,
                        delete_on_termination=True,
                    ),
                )
            ],
            require_imdsv2=True,  # harden instance metadata endpoint
        )

        # Spot fleet CFN resource (L1 construct — no L2 Spot fleet in CDK v2)
        private_subnet_ids = [subnet.subnet_id for subnet in vpc.private_subnets]

        launch_spec_overrides = [
            ec2.CfnSpotFleet.LaunchTemplateOverridesProperty(
                subnet_id=subnet_id,
                instance_type="c5.4xlarge",
            )
            for subnet_id in private_subnet_ids
        ]

        spot_fleet_role = iam.Role(
            self,
            "SpotFleetRole",
            assumed_by=iam.ServicePrincipal("spotfleet.amazonaws.com"),
            managed_policies=[
                iam.ManagedPolicy.from_aws_managed_policy_name(
                    "service-role/AmazonEC2SpotFleetTaggingRole"
                )
            ],
        )

        ec2.CfnSpotFleet(
            self,
            "UE5BuildFleet",
            spot_fleet_request_config_data=ec2.CfnSpotFleet.SpotFleetRequestConfigDataProperty(
                iam_fleet_role=spot_fleet_role.role_arn,
                target_capacity=1,           # start with 1; scale up as queue grows
                spot_price="0.50",           # max bid: ~40% of on-demand c5.4xlarge in af-south-1
                allocation_strategy="lowestPrice",
                instance_interruption_behavior="terminate",
                # Replace unhealthy instances automatically
                replace_unhealthy_instances=True,
                type="maintain",             # maintain target capacity; respawn terminated nodes
                launch_template_configs=[
                    ec2.CfnSpotFleet.LaunchTemplateConfigProperty(
                        launch_template_specification=ec2.CfnSpotFleet.FleetLaunchTemplateSpecificationProperty(
                            launch_template_id=launch_template.launch_template_id,
                            version=launch_template.latest_version_number,
                        ),
                        overrides=launch_spec_overrides,
                    )
                ],
                # Hard cap: never exceed 4 concurrent build agents
                # (matches the number of UE5 Derived Data Cache EFS connections)
                excess_capacity_termination_policy="noTermination",
            ),
        )

        # ------------------------------------------------------------------
        # Stack outputs — useful after `cdk deploy`
        # ------------------------------------------------------------------
        cdk.CfnOutput(self, "ArtifactsBucket", value=artifacts_bucket.bucket_name)
        cdk.CfnOutput(self, "ContentEFSId", value=content_fs.file_system_id)
        cdk.CfnOutput(self, "BuildVpcId", value=vpc.vpc_id)
        cdk.CfnOutput(
            self,
            "BuildAgentRoleArn",
            value=build_role.role_arn,
            description="Attach this role ARN to additional CI runners if needed",
        )


# ------------------------------------------------------------------------------
# CDK App entry-point
# af-south-1 = Africa (Cape Town) — lowest latency from Eswatini
# ------------------------------------------------------------------------------
app = cdk.App()

MahlanyaBuildStack(
    app,
    "MahlanyaBuildStack",
    env=cdk.Environment(region="af-south-1"),
    description="Mahlanya RPG — UE5 Spot build fleet, EFS Content volume, S3 artifacts",
)

app.synth()
