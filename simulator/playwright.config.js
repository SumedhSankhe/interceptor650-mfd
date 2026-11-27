const { defineConfig } = require('@playwright/test');

const port = process.env.SIMULATOR_TEST_PORT || '18765';
const url = 'http://127.0.0.1:' + port;
const python = process.env.SIMULATOR_PYTHON || (process.platform === 'win32' ? 'python' : 'python3');

module.exports = defineConfig({
  testDir: './browser',
  workers: 1,
  outputDir: './build/browser-results',
  use: {
    browserName: 'chromium',
    channel: 'chromium',
    baseURL: url,
    viewport: { width: 1100, height: 680 },
    deviceScaleFactor: 1,
  },
  webServer: process.env.SIMULATOR_USE_RUNNING_SERVER === '1' ? undefined : {
    command: '"' + python + '" server.py --port ' + port,
    url,
  },
});
