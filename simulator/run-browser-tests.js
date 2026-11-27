const { spawn } = require('node:child_process');
const { once } = require('node:events');
const { createInterface } = require('node:readline');

async function startServer(server) {
  const lines = createInterface({ input: server.stdout });
  const timer = setTimeout(() => server.kill(), 10000);
  try {
    for await (const line of lines) {
      const match = /^Open http:\/\/127\.0\.0\.1:(\d+)$/.exec(line);
      if (match) return match[1];
    }
    throw new Error('Simulator exited or timed out before reporting its loopback port');
  } finally {
    clearTimeout(timer);
    lines.close();
  }
}

async function run() {
  const env = { ...process.env };
  let server;
  let runner;
  const stop = () => {
    runner?.kill();
    server?.kill();
  };
  process.once('SIGINT', stop);
  process.once('SIGTERM', stop);
  try {
    if (env.SIMULATOR_USE_RUNNING_SERVER !== '1') {
      const python = env.SIMULATOR_PYTHON || (process.platform === 'win32' ? 'python' : 'python3');
      server = spawn(python, ['server.py', '--port', env.SIMULATOR_TEST_PORT || '0'], {
        stdio: ['ignore', 'pipe', 'inherit'],
      });
      // Handle spawn errors immediately while waiting for stdout.
      const failed = once(server, 'error').then(([error]) => { throw error; });
      env.SIMULATOR_TEST_PORT = await Promise.race([startServer(server), failed]);
    }

    runner = spawn(process.execPath, [require.resolve('@playwright/test/cli'), 'test', ...process.argv.slice(2)], {
      stdio: 'inherit',
      env,
    });
    const [code] = await once(runner, 'exit');
    process.exitCode = code ?? 1;
  } finally {
    if (server && server.exitCode === null && server.signalCode === null && server.pid) {
      const exited = once(server, 'exit');
      server.kill();
      await exited;
    }
    process.removeListener('SIGINT', stop);
    process.removeListener('SIGTERM', stop);
  }
}

run().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
