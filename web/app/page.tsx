import Link from 'next/link';
import HeightmapViewer from '../components/HeightmapViewer';

export default function Page() {
  return (
    <main style={{ width: '100vw', height: '100vh', background: '#0a0a0a', position: 'relative' }}>
      <HeightmapViewer />
      <Link
        href="/access"
        style={{
          position: 'absolute',
          top: 12,
          right: 12,
          padding: '8px 12px',
          background: 'rgba(0,0,0,0.55)',
          color: '#eaeaea',
          font: '13px system-ui, sans-serif',
          borderRadius: 4,
          textDecoration: 'none',
          zIndex: 10,
        }}
      >
        Get supporter build &rarr;
      </Link>
    </main>
  );
}
