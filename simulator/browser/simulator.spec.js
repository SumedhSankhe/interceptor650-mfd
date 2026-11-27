const { mkdir } = require('node:fs/promises');
const path = require('node:path');
const { expect, test } = require('@playwright/test');

const displayPath = '/display.svg';
const displaySelector = '#display';
const labels = {
  mode: 'Display mode',
  scenario: 'Telemetry fixture',
  nav: 'Navigation fixture',
  unit: 'Temperature units',
};

async function openSimulator(page, action = () => page.goto('/')) {
  const response = page.waitForResponse((response) => new URL(response.url()).pathname === displayPath);
  await action();
  await expect(page.getByRole('status')).toHaveText('Fixture rendered');
  const image = page.locator(displaySelector);
  expect(await image.evaluate((element) => element.naturalWidth)).toBe(466);
  const loaded = await response;
  expect(loaded.status()).toBe(200);
  expect(loaded.headers()['content-type']).toContain('image/svg+xml');
  await expect(image).toHaveJSProperty('currentSrc', loaded.url());
  return { svg: await loaded.text(), url: loaded.url() };
}

async function select(page, name, value) {
  return openSimulator(page, () => page.getByLabel(labels[name]).selectOption(value));
}

function checkText(svg, present, absent = []) {
  for (const label of present) expect(svg).toContain('>' + label + '</text>');
  for (const label of absent) expect(svg).not.toContain('>' + label + '</text>');
}

const browserErrors = new WeakMap();

test.beforeEach(async ({ page }) => {
  const errors = [];
  page.on('console', (message) => {
    if (message.type() === 'error') errors.push('Console: ' + message.text());
  });
  page.on('pageerror', (error) => errors.push('JavaScript: ' + error.message));
  page.on('requestfailed', (request) => {
    errors.push(request.url() + ' ' + request.failure()?.errorText);
  });
  page.on('response', (response) => {
    if (response.status() >= 400) errors.push(response.status() + ' ' + response.url());
  });
  browserErrors.set(page, errors);
});

test.afterEach(async ({ page }) => {
  expect(browserErrors.get(page)).toEqual([]);
});

test('switches all display modes and converts temperatures in both directions', async ({ page }) => {
  checkText((await openSimulator(page)).svg, ['TELEMETRY', '108°C', '32°C', '14.2V', 'INTAKE'], ['400 m']);
  const fahrenheit = (await select(page, 'unit', 'F')).svg;
  checkText(fahrenheit, ['226°F', '90°F', '14.2V'], ['108°C', '32°C']);
  checkText((await select(page, 'mode', 'navigation')).svg,
    ['NAVIGATION', '400 m', 'TURN RIGHT'], ['226°F', '14.2V']);
  checkText((await select(page, 'mode', 'hybrid')).svg,
    ['HYBRID', '400 m', '226°F', '14.2V'], ['INTAKE']);
  checkText((await select(page, 'unit', 'C')).svg, ['108°C', '14.2V'], ['226°F']);
  checkText((await select(page, 'mode', 'telemetry')).svg, ['TELEMETRY', '108°C'], ['400 m']);
});

test('changes telemetry and navigation independently and restores each source', async ({ page }) => {
  await openSimulator(page);
  await select(page, 'mode', 'hybrid');
  checkText((await select(page, 'scenario', 'stale')).svg,
    ['STALE', '—', '400 m'], ['108°C', '14.2V']);
  checkText((await select(page, 'nav', 'rerouting')).svg,
    ['REROUTING', 'NO GUIDANCE', 'STALE'], ['400 m', 'TURN RIGHT', '108°C']);
  checkText((await select(page, 'scenario', 'unsupported')).svg,
    ['UNSUPPORTED', '14.2V', 'REROUTING'], ['108°C', '400 m']);
  const staleNav = (await select(page, 'nav', 'stale')).svg;
  checkText(staleNav, ['STALE', 'NO GUIDANCE', 'UNSUPPORTED', '14.2V'], ['TURN RIGHT', '400 m']);
  checkText((await select(page, 'scenario', 'disconnected')).svg,
    ['MISSING', 'STALE', 'NO GUIDANCE'], ['UNSUPPORTED', '108°C', '14.2V', '400 m']);
  checkText((await select(page, 'nav', 'disconnected')).svg,
    ['DISCONNECTED', 'MISSING', 'NO GUIDANCE'], ['400 m', 'TURN RIGHT']);
  checkText((await select(page, 'scenario', 'normal')).svg,
    ['108°C', '14.2V', 'DISCONNECTED'], ['400 m']);
  checkText((await select(page, 'nav', 'active')).svg, ['400 m', '108°C', '14.2V'], ['DISCONNECTED']);
});

const evidence = [
  ['telemetry', 'telemetry', 'normal', 'active'],
  ['navigation', 'navigation', 'normal', 'active'],
  ['hybrid', 'hybrid', 'normal', 'active'],
  ['telemetry-stale', 'hybrid', 'stale', 'active'],
  ['telemetry-unsupported', 'hybrid', 'unsupported', 'active'],
  ['telemetry-disconnected', 'hybrid', 'disconnected', 'active'],
  ['navigation-rerouting', 'hybrid', 'normal', 'rerouting'],
  ['navigation-stale', 'hybrid', 'normal', 'stale'],
  ['navigation-disconnected', 'hybrid', 'normal', 'disconnected'],
  ['combined-degraded', 'hybrid', 'stale', 'disconnected'],
];

async function saveEvidence(page, name) {
  const directory = process.env.SIMULATOR_SCREENSHOTS;
  if (directory) {
    await mkdir(directory, { recursive: true });
    await page.screenshot({ path: path.join(directory, name + '.png'), fullPage: true });
  }
}

for (const [name, mode, scenario, nav] of evidence) {
  test('renders ' + name + ' within the circular display', async ({ page }) => {
    let display = await openSimulator(page);
    for (const [field, value] of Object.entries({ mode, scenario, nav })) {
      if (await page.getByLabel(labels[field]).inputValue() !== value) {
        display = await select(page, field, value);
      }
    }
    const image = page.locator(displaySelector);
    expect(await image.boundingBox()).toMatchObject({ width: 466, height: 466 });
    const alpha = await image.evaluate((element) => {
      const canvas = document.createElement('canvas');
      canvas.width = canvas.height = 466;
      const context = canvas.getContext('2d');
      context.drawImage(element, 0, 0);
      return [[0, 0], [465, 0], [0, 465], [465, 465], [233, 233]]
        .map(([x, y]) => context.getImageData(x, y, 1, 1).data[3]);
    });
    expect(alpha).toEqual([0, 0, 0, 0, 255]);
    await saveEvidence(page, name);

    // Measure the loaded SVG with Chromium's actual font metrics.
    await page.goto(display.url);
    const outsideFace = await page.locator('svg g').evaluate((group) => {
      const face = group.previousElementSibling;
      const cx = face.cx.baseVal.value;
      const cy = face.cy.baseVal.value;
      const radius = face.r.baseVal.value;
      return Array.from(group.children).filter((element) => {
        const { x, y, width, height } = element.getBBox();
        return [x, x + width].some((left) => [y, y + height]
          .some((top) => Math.hypot(left - cx, top - cy) > radius));
      }).map((element) => element.textContent || element.outerHTML);
    });
    expect(outsideFace).toEqual([]);
  });
}

test('fits a narrow viewport and keeps controls usable below the display', async ({ page }) => {
  await page.setViewportSize({ width: 280, height: 760 });
  await openSimulator(page);
  checkText((await select(page, 'mode', 'hybrid')).svg, ['HYBRID', '400 m', '108°C']);
  const image = await page.locator(displaySelector).boundingBox();
  const controls = await page.locator('#controls').boundingBox();
  expect(image.width).toBeLessThan(466);
  expect(Math.abs(image.width - image.height)).toBeLessThan(1);
  expect(controls.y).toBeGreaterThan(image.y + image.height);
  expect(controls.x + controls.width).toBeLessThanOrEqual(image.x + image.width);
  expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBe(280);
  await saveEvidence(page, 'narrow-hybrid');
});
