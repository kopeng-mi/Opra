import { serve } from 'bun';
import path from 'node:path';
import { readFile, stat } from 'node:fs/promises';

const ROOT = import.meta.dir;
const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.png': 'image/png',
  '.json': 'application/json',
};

const server = serve({
  port: 8099,
  async fetch(req) {
    const url = new URL(req.url);
    let filePath = path.join(ROOT, url.pathname === '/' ? 'viewer.html' : url.pathname);
    try {
      const info = await stat(filePath);
      if (info.isDirectory()) filePath = path.join(filePath, 'viewer.html');
      const content = await readFile(filePath);
      const ext = path.extname(filePath);
      return new Response(content, {
        headers: { 'Content-Type': MIME[ext] || 'application/octet-stream' },
      });
    } catch {
      return new Response('Not found', { status: 404 });
    }
  },
});

console.log(`Yard viewer server listening on http://localhost:${server.port}`);
