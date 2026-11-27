const { mkdir } = require('node:fs/promises');
const path = require('node:path');
const { expect, test } = require('@playwright/test');

const displayPath = '/snapshot.json';
const displaySelector = '#display';
const labels = {
  mode: 'Display mode',
  scenario: 'Telemetry fixture',
  nav: 'Navigation fixture',
  unit: 'Temperature units',
  connection: 'Navigation session',
  maneuver: 'Maneuver',
  exit_number: 'Roundabout exit',
  distance: 'Distance (m)',
  age: 'Elapsed since snapshot (ms)',
  distance_unit: 'Distance units',
};

async function openSimulator(page, action = () => page.goto('/')) {
  const response = page.waitForResponse((response) => new URL(response.url()).pathname === displayPath);
  await action();
  await expect(page.getByRole('status')).toHaveText('Fixture rendered');
  const image = page.locator(displaySelector);
  await expect(image).toHaveJSProperty('naturalWidth', 466);
  const loaded = await response;
  expect(loaded.status()).toBe(200);
  expect(loaded.headers()['content-type']).toContain('application/json');
  const { svg, diagnostics } = await loaded.json();
  const url = await image.evaluate((element) => element.currentSrc);
  expect(url).toMatch(/^blob:/);
  expect(await page.evaluate(async (source) => (await fetch(source)).text(), url)).toBe(svg);
  await expect(image).toHaveAttribute('data-sequence', String(diagnostics.sequence));
  await expect(page.locator('#ride-details')).toHaveAttribute('data-sequence', String(diagnostics.sequence));
  return { svg, url, diagnostics };
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

    await checkGeometry(page, display);
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

async function input(page, name, value) {
  return openSimulator(page, async () => {
    const field = page.getByLabel(labels[name]);
    await field.fill(String(value));
    await field.press('Tab');
  });
}

async function checkGeometry(page, display) {
  // A separate page leaves the control state intact for subsequent interactions.
  const svg = await page.context().newPage();
  try {
    await svg.goto(display.url);
    const outsideFace = await svg.locator('svg g').evaluate((group) => {
      const face = group.previousElementSibling;
      const cx = face.cx.baseVal.value;
      const cy = face.cy.baseVal.value;
      const radius = face.r.baseVal.value;
      return Array.from(group.children).filter((element) => {
        const { x, y, width, height } = element.getBBox();
        const style = getComputedStyle(element);
        const stroke = style.stroke === 'none' ? 0 : parseFloat(style.strokeWidth) / 2;
        const matrix = element.transform.baseVal.consolidate()?.matrix || new DOMMatrix();
        return [x - stroke, x + width + stroke].some((left) => [y - stroke, y + height + stroke]
          .some((top) => {
            const point = new DOMPoint(left, top).matrixTransform(matrix);
            return Math.hypot(point.x - cx, point.y - cy) > radius;
          }));
      }).map((element) => element.textContent || element.outerHTML);
    });
    expect(outsideFace).toEqual([]);
  } finally {
    await svg.close();
  }
}

const maneuvers = [
  ['straight', 'STRAIGHT AHEAD'],
  ['left', 'TURN LEFT'],
  ['right', 'TURN RIGHT'],
  ['uturn', 'U-TURN'],
  ['roundabout', 'ROUNDABOUT · EXIT 2'],
  ['slight-left', 'SLIGHT LEFT'],
  ['slight-right', 'SLIGHT RIGHT'],
  ['keep-left', 'KEEP LEFT'],
  ['keep-right', 'KEEP RIGHT'],
];

test('renders every maneuver with a distinct glyph and fits its labels inside the face', async ({ page }) => {
  await openSimulator(page);
  await select(page, 'mode', 'navigation');
  const glyphs = new Set();
  for (const [maneuver, caption] of maneuvers) {
    const display = await select(page, 'maneuver', maneuver);
    checkText(display.svg, [caption, '400 m']);
    const glyph = display.svg.match(/<path d="[^"]+"(?: transform="[^"]+")? fill="none"/)?.[0];
    expect(glyph).toBeTruthy();
    if (maneuver === 'roundabout') expect(glyph).toContain('A32 32 0 1 0');
    expect(glyphs.has(glyph)).toBe(false);
    glyphs.add(glyph);
    await saveEvidence(page, 'maneuver-' + maneuver);
    await checkGeometry(page, display);
  }
  let display = await input(page, 'distance', 4294967295);
  checkText(display.svg, ['4294967 km', 'KEEP RIGHT']);
  await checkGeometry(page, display);
  await select(page, 'maneuver', 'roundabout');
  display = await input(page, 'exit_number', 255);
  checkText(display.svg, ['ROUNDABOUT · EXIT 255', '255']);
  await checkGeometry(page, display);
  checkText((await input(page, 'exit_number', 0)).svg, ['ROUNDABOUT'], ['ROUNDABOUT · EXIT 255', '255']);
});

test('uses independent distance and temperature units and accepts zero distance', async ({ page }) => {
  await openSimulator(page);
  await select(page, 'mode', 'hybrid');
  checkText((await select(page, 'distance_unit', 'imperial')).svg, ['1310 ft', '108°C']);
  checkText((await select(page, 'unit', 'F')).svg, ['1310 ft', '226°F']);
  checkText((await input(page, 'distance', 2000)).svg, ['1.2 mi', '226°F']);
  checkText((await select(page, 'distance_unit', 'metric')).svg, ['2.0 km', '226°F']);
  checkText((await input(page, 'distance', 0)).svg, ['0 m', '226°F']);
});

test('clears obsolete guidance, replays a new session, and observes the timeout boundary', async ({ page }) => {
  await openSimulator(page);
  await select(page, 'mode', 'hybrid');
  checkText((await input(page, 'age', 2999)).svg, ['400 m', '108°C']);
  checkText((await input(page, 'age', 3000)).svg, ['NO GUIDANCE', 'STALE', '108°C'], ['400 m', 'TURN RIGHT']);
  await saveEvidence(page, 'elapsed-stale');
  checkText((await input(page, 'age', 0)).svg, ['400 m', '108°C']);
  checkText((await select(page, 'nav', 'rerouting')).svg, ['REROUTING', '108°C'], ['400 m']);
  checkText((await select(page, 'nav', 'arrived')).svg, ['ARRIVED', '108°C'], ['400 m', 'TURN RIGHT']);
  await saveEvidence(page, 'arrived');
  checkText((await select(page, 'nav', 'idle')).svg, ['IDLE'], ['400 m']);
  checkText((await select(page, 'nav', 'unavailable')).svg, ['UNAVAILABLE'], ['400 m']);
  checkText((await select(page, 'nav', 'disconnected')).svg, ['DISCONNECTED', '108°C'], ['400 m']);
  await select(page, 'connection', 'waiting');
  checkText((await select(page, 'nav', 'active')).svg, ['UNAVAILABLE', '108°C'], ['400 m']);
  checkText((await select(page, 'connection', 'reconnected')).svg, ['400 m', 'TURN RIGHT', '108°C']);
  checkText((await select(page, 'scenario', 'disconnected')).svg, ['400 m', 'MISSING'], ['108°C']);
});

async function checkDiagnostics(page, fields) {
  for (const [field, value] of Object.entries(fields)) {
    await expect(page.locator('#ride-' + field)).toHaveText(String(value));
  }
}

async function ride(page, scenario, fields) {
  const display = await openSimulator(page, () => page.getByLabel('Ride scenario').selectOption(scenario));
  await checkDiagnostics(page, fields);
  return display;
}

test('records one ignition session across engine stops, restarts, ECU and phone gaps', async ({ page }) => {
  await openSimulator(page);
  await expect(page.locator('#ride-rpm')).toHaveText('0');
  await expect(page.locator('#ride-rpm_quality')).toHaveText('simulated');
  await expect(page.locator('#ride-engine')).toHaveText('stopped');
  await expect(page.locator('#ride-session')).toHaveText('1');
  await ride(page, 'start', { engine: 'running', session: 1, records: 2 });
  await ride(page, 'running', { engine: 'running', session: 1, records: 7, sync_requests: 1 });
  await ride(page, 'stall', { rpm: 0, engine: 'stopped', session: 1, records: 3, sync_requests: 1 });
  await saveEvidence(page, 'ride-stall');
  await ride(page, 'restart', { engine: 'running', session: 1, records: 4, recording: 'recording' });
  await ride(page, 'kill-switch', { rpm: 0, engine: 'stopped', session: 1, sync_requests: 1 });
  await ride(page, 'ecu-loss', { rpm: '—', rpm_quality: 'missing', engine: 'unknown', session: 1, sync_requests: 0 });
  await ride(page, 'ecu-reconnect', { engine: 'running', session: 1, records: 4 });
  await select(page, 'mode', 'hybrid');
  const display = await select(page, 'nav', 'disconnected');
  checkText(display.svg, ['DISCONNECTED', '108°C', '14.2V'], ['400 m']);
  const disconnected = await ride(page, 'phone-independent', { engine: 'running', session: 1, records: 7 });
  expect(disconnected.svg).toBe(display.svg);
  await saveEvidence(page, 'ride-without-phone');
});

test('reports storage failure, resumes writing, and starts a new session after a torn record', async ({ page }) => {
  await openSimulator(page);
  await select(page, 'mode', 'hybrid');
  await ride(page, 'storage-failure', {
    engine: 'running', recording: 'degraded', session: 1, storage_errors: 1, last_error: 'full', records: 2,
  });
  await saveEvidence(page, 'ride-storage-full');
  await ride(page, 'storage-recovery', { recording: 'recording', session: 1, records: 3, storage_errors: 1 });
  await ride(page, 'power-loss', {
    rpm: '—', engine: 'unknown', recording: 'recording', session: 2, elapsed_ms: 0, records: 1,
    recovery: 'Truncated tail; previous session interrupted', recovered_records: 2,
  });
  await saveEvidence(page, 'ride-power-recovery');
});

test('replays one integrated timeline for rendering, recording, failures and reboot', async ({ page }) => {
  await openSimulator(page);
  const frame = async (step) => openSimulator(page,
    () => page.getByLabel('Replay step').selectOption(String(step)));
  const initial = await openSimulator(page,
    () => page.getByLabel('Simulation mode').selectOption('timeline'));
  checkText(initial.svg, ['95°C', '12.6V', 'TELEMETRY']);
  expect(initial.diagnostics).toMatchObject({ oil_c: 95, voltage_v: 12.6, rpm: 0, session: 1 });
  await saveEvidence(page, 'runtime-telemetry');
  await openSimulator(page, () => page.getByRole('button', { name: 'Next replay step' }).click());
  await expect(page.getByLabel('Replay step')).toHaveValue('1');
  await checkDiagnostics(page, { engine: 'running', session: 1, records: 2 });
  await select(page, 'mode', 'hybrid');

  const hybrid = await frame(4);
  checkText(hybrid.svg, ['ROUNDABOUT · EXIT 2', '99°C', '14.1V']);
  expect(hybrid.diagnostics).toMatchObject({ oil_c: 99, voltage_v: 14.1, engine: 'running', phone: 'connected' });
  await checkGeometry(page, hybrid);
  await saveEvidence(page, 'runtime-hybrid');

  const stall = await frame(5);
  checkText(stall.svg, ['100°C', '12.6V', 'ROUNDABOUT · EXIT 2']);
  await checkDiagnostics(page, { rpm: 0, engine: 'stopped', recording: 'recording', session: 1, sync_requests: 1 });
  await saveEvidence(page, 'runtime-stall');
  await frame(6);
  await checkDiagnostics(page, { engine: 'running', session: 1 });

  const ecu = await frame(7);
  checkText(ecu.svg, ['MISSING', '12.6V', 'ROUNDABOUT · EXIT 2'], ['100°C']);
  expect(ecu.diagnostics).toMatchObject({ oil_c: null, voltage_v: 12.6, engine: 'unknown', ecu: 'disconnected' });
  await saveEvidence(page, 'runtime-ecu-gap');

  const phone = await frame(8);
  checkText(phone.svg, ['DISCONNECTED', '103°C', '14.1V'], ['400 m']);
  await checkDiagnostics(page, { phone: 'disconnected', engine: 'running', session: 1 });
  await saveEvidence(page, 'runtime-phone-gap');

  await frame(13);
  await checkDiagnostics(page, { engine: 'stopped', queue_depth: 4, queue_peak: 4, dropped_samples: 1, session: 1 });
  await saveEvidence(page, 'runtime-saturation');

  const degraded = await frame(14);
  checkText(degraded.svg, ['DISCONNECTED', 'MISSING', '12.6V'], ['400 m']);
  await checkDiagnostics(page, { recording: 'degraded', engine: 'unknown', ecu: 'disconnected', phone: 'disconnected' });
  await saveEvidence(page, 'runtime-degraded');
  await frame(15);
  await checkDiagnostics(page, { recording: 'recording', session: 1 });

  await frame(16);
  const reboot = await frame(17);
  checkText(reboot.svg, ['DISCONNECTED', 'MISSING'], ['14.1V', '400 m']);
  await checkDiagnostics(page, { engine: 'unknown', session: 2, settings: 'restored', elapsed_ms: 0 });
  expect(reboot.diagnostics.recovery).toContain('Truncated tail');
  expect(reboot.diagnostics.recovered_records).toBeGreaterThan(0);
  await expect(page.getByRole('button', { name: 'Next replay step' })).toBeDisabled();
  await saveEvidence(page, 'runtime-recovery');
});

test('keeps integrated replay and diagnostics usable on a narrow screen', async ({ page }) => {
  await page.setViewportSize({ width: 280, height: 760 });
  await openSimulator(page);
  await openSimulator(page, () => page.getByLabel('Simulation mode').selectOption('timeline'));
  await openSimulator(page, () => page.getByLabel('Replay step').selectOption('4'));
  await expect(page.getByRole('button', { name: 'Next replay step' })).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBe(280);
  await checkDiagnostics(page, { engine: 'running', session: 1 });
  await saveEvidence(page, 'runtime-replay-narrow');
});
