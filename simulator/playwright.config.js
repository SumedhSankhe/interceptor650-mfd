const { defineConfig } = require('@playwright/test');

const port = process.env.SIMULATOR_TEST_PORT || '18765';

module.exports = defineConfig({
  testDir: './browser',
  workers: 1,
  outputDir: './build/browser-results',
  reporter: process.env.CI
    ? [['line'], ['html', { outputFolder: 'build/playwright-report', open: 'never' }]]
    : 'list',
  use: {
    browserName: 'chromium',
    channel: 'chromium',
    baseURL: 'http://127.0.0.1:' + port,
    viewport: { width: 1100, height: 680 },
    deviceScaleFactor: 1,
    screenshot: 'only-on-failure',
    trace: 'retain-on-failure',
  },
});
