/* AudioScope by BerryStudio — 0.7-alpha UI shell.
   Vanilla JS. Data is injected by `audioscope-alpha ui`; otherwise a
   deterministic synthetic snapshot is used so the UI can be reviewed offline. */
(function () {
  'use strict';

  const $ = (sel, root) => (root || document).querySelector(sel);
  const $$ = (sel, root) => Array.from((root || document).querySelectorAll(sel));
  const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
  const esc = (s) => String(s == null ? '' : s).replace(/[&<>"']/g, (c) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  })[c]);
  const minus = (v, digits) => (v < 0 ? '−' : '') + Math.abs(v).toFixed(digits);
  const fmt = (v, digits) => Number(v).toFixed(digits);
  const DB = (v) => minus(v, 1) + ' dBFS';
  const DEG = (v) => minus(v, 1) + '°';

  /* ------------------------------------------------------------ fallback data */
  function synthWave(n) {
    const out = [];
    for (let i = 0; i < n; i++) {
      const t = i / 48000;
      out.push(0.5 * Math.sin(2 * Math.PI * 1000 * t) +
               0.0004 * Math.sin(2 * Math.PI * 2000 * t) +
               0.0003 * Math.sin(2 * Math.PI * 3000 * t));
    }
    return out;
  }
  function synthSpectrum() {
    const freqs = [], db = [];
    for (let i = 0; i < 220; i++) {
      const f = 20 * Math.pow(1000, i / 219);
      freqs.push(f);
      let v = -96 + Math.random() * 2;
      if (f > 900 && f < 1120) v = -8;
      if (f > 1900 && f < 2120) v = -66;
      if (f > 2900 && f < 3120) v = -70;
      if (f > 55 && f < 65) v = -78;
      db.push(v);
    }
    return { freqs, db, thd: 0.1, thdn: 0.1212, snr: 63.29, sinad: 58.33, sfdr: 46.14, enob: 9.4 };
  }
  function synthXY(phase, gain) {
    const pts = [];
    const g = Math.pow(10, gain / 20);
    const ph = phase * Math.PI / 180;
    for (let i = 0; i < 1600; i++) {
      const t = i / 48000;
      pts.push([0.5 * Math.sin(2 * Math.PI * 1000 * t),
                0.5 * g * Math.sin(2 * Math.PI * 1000 * t + ph)]);
    }
    return { points: pts, phase_deg: phase, gain_db: gain, freq_hz: 1000, drift_ppm: 0 };
  }

  const FALLBACK = {
    meta: {
      version: '0.7.0-alpha.1',
      platform: 'macOS · CoreAudio',
      mode: 'Inst',
      device: 'Studio 1824c',
      rate: 48000,
      channels: 2,
      source: 'synthetic snapshot',
      timestamp: '2026-11-07 13:57'
    },
    scope: {
      waveform: synthWave(720),
      peak_dbfs: -6.0, rms_dbfs: -9.1, freq_hz: 1000, crest: 3.0,
      vpp: 1.0, clip: 0, channel: 0
    },
    spectrum: synthSpectrum(),
    xy: synthXY(45, 3),
    channels: [
      { label: 'CH1', peak_dbfs: -6.0, rms_dbfs: -9.1, freq_hz: 1000, vpp: 1.0000, clip: 0, verdict: 'PASS' },
      { label: 'CH2', peak_dbfs: -6.2, rms_dbfs: -9.3, freq_hz: 1000, vpp: 0.9800, clip: 0, verdict: 'PASS' },
      { label: 'CH3', peak_dbfs: -96.0, rms_dbfs: -102.0, freq_hz: 0, vpp: 0.0000, clip: 0, verdict: '—' },
      { label: 'CH4', peak_dbfs: -96.4, rms_dbfs: -102.2, freq_hz: 0, vpp: 0.0000, clip: 0, verdict: '—' },
      { label: 'CH5', peak_dbfs: -95.8, rms_dbfs: -101.8, freq_hz: 0, vpp: 0.0000, clip: 0, verdict: '—' },
      { label: 'CH6', peak_dbfs: -96.1, rms_dbfs: -102.0, freq_hz: 0, vpp: 0.0000, clip: 0, verdict: '—' },
      { label: 'CH7', peak_dbfs: -96.2, rms_dbfs: -102.1, freq_hz: 0, vpp: 0.0000, clip: 0, verdict: '—' },
      { label: 'CH8', peak_dbfs: -96.3, rms_dbfs: -102.2, freq_hz: 0, vpp: 0.0000, clip: 0, verdict: '—' }
    ],
    repair: {
      template: 'tape', label: 'Tape deck', verdict: 'PASS',
      steps: [
        { label: 'Wow', value: 0.2121, unit: '% RMS', pass: true },
        { label: 'Flutter', value: 0.0587, unit: '% RMS', pass: true },
        { label: 'Azimuth', value: 10.0, unit: 'deg', pass: true },
        { label: 'THD', value: 0.1, unit: '%', pass: true }
      ]
    },
    reports: [
      { name: 'repair-tape.pdf', kind: '维修计划', verdict: 'PASS', time: '13:57' },
      { name: 'run-20261107-043914.pdf', kind: '测量报告', verdict: 'WARN', time: '04:39' },
      { name: 'run-20261107-043823.pdf', kind: '测量报告', verdict: 'WARN', time: '04:38' }
    ],
    calibration: {
      id: 'none', confidence: 'ASSUMED', fs_vrms: 0.0,
      note: '未校准：只显示 dBFS；电压需要 measured / datasheet / assumed。'
    }
  };

  const DATA = window.AUDIOSCOPE_DATA || FALLBACK;
  if (!DATA.meta) DATA.meta = FALLBACK.meta;
  if (!DATA.scope) DATA.scope = FALLBACK.scope;
  if (!DATA.spectrum) DATA.spectrum = FALLBACK.spectrum;
  if (!DATA.xy) DATA.xy = FALLBACK.xy;
  if (!DATA.channels) DATA.channels = FALLBACK.channels;
  if (!DATA.repair) DATA.repair = FALLBACK.repair;
  if (!DATA.reports) DATA.reports = FALLBACK.reports;
  if (!DATA.calibration) DATA.calibration = FALLBACK.calibration;

  /* ---------------------------------------------------------------- icons */
  const ICONS = {
    scope: '<path d="M2 10h3l2-6 3 12 3-9 2 3h3"/>',
    spectrum: '<path d="M3 16V9m4 7V5m4 11V8m4 8V3m4 13V7"/>',
    xy: '<circle cx="10" cy="10" r="7"/><path d="M10 3v14M3 10h14"/>',
    measure: '<path d="M3 6h14M3 14h14"/><circle cx="7" cy="6" r="2"/><circle cx="13" cy="14" r="2"/>',
    repair: '<path d="M14.5 3.5a4 4 0 0 0-5 5L4 14l2 2 5.5-5.5a4 4 0 0 0 5-5l-2.5 2.5-2-2z"/>',
    reports: '<path d="M5 2h7l3 3v13H5z"/><path d="M12 2v4h4M7 9h6M7 12h6M7 15h4"/>',
    calibration: '<circle cx="10" cy="10" r="7"/><circle cx="10" cy="10" r="3"/><path d="M10 1v3M10 16v3M1 10h3M16 10h3"/>'
  };
  const TABS = [
    { id: 'scope', label: '波形', icon: 'scope' },
    { id: 'spectrum', label: '频谱', icon: 'spectrum' },
    { id: 'xy', label: '相位', icon: 'xy' },
    { id: 'measure', label: '测量', icon: 'measure' },
    { id: 'repair', label: '维修', icon: 'repair' },
    { id: 'reports', label: '报告', icon: 'reports' },
    { id: 'calibration', label: '校准', icon: 'calibration' }
  ];

  function icon(name, cls) {
    return '<svg class="' + (cls || '') + '" viewBox="0 0 20 20" fill="none" stroke="currentColor" ' +
      'stroke-width="1.15" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' +
      (ICONS[name] || '') + '</svg>';
  }
  function mark(size) {
    return '<svg class="brand-mark" width="' + size + '" height="' + size + '" viewBox="0 0 44 44" ' +
      'fill="none" stroke="var(--accent)" stroke-width="1.7" stroke-linecap="round" aria-hidden="true">' +
      '<path d="M22 5a17 17 0 0 1 16 11"/><path d="M39 27a17 17 0 0 1-12 12"/>' +
      '<path d="M13 38A17 17 0 0 1 5 25"/><path d="M6 17A17 17 0 0 1 15 6"/>' +
      '<circle cx="22" cy="22" r="3" fill="var(--accent)" stroke="none"/></svg>';
  }

  /* ---------------------------------------------------------------- state */
  const query = new URLSearchParams(location.search);
  const hash = (location.hash || '#scope').slice(1);
  const initialModal = (hash === 'about' || hash === 'settings') ? hash : null;
  const state = {
    tab: TABS.some((t) => t.id === hash) ? hash : 'scope',
    theme: query.get('theme') || document.documentElement.getAttribute('data-theme') || 'dark',
    held: false,
    spectrumMode: 'log',
    persistence: '2 s',
    modal: null
  };
  document.documentElement.setAttribute('data-theme', state.theme === 'light' ? 'light' : 'dark');

  /* --------------------------------------------------------------- header */
  function renderHeader() {
    const m = DATA.meta;
    $('#topbar').innerHTML =
      '<div class="brand">' + mark(44) +
        '<div><div class="brand-word">AUDIOSCOPE</div>' +
        '<div class="brand-sub">BERRYSTUDIO / ' + esc(m.version) + '</div></div>' +
      '</div>' +
      '<div class="top-actions">' +
        '<button class="btn ghost" data-action="theme">' + (state.theme === 'dark' ? '深色' : '浅色') + '</button>' +
        '<button class="btn ghost" data-modal="about">关于</button>' +
        '<button class="btn ghost" data-modal="settings">设置</button>' +
        '<button class="btn" data-action="snapshot">快照</button>' +
      '</div>';
  }

  /* ----------------------------------------------------------------- rail */
  function renderRail() {
    $('#rail').innerHTML = TABS.map((t) =>
      '<button class="tab' + (t.id === state.tab ? ' active' : '') + '" data-tab="' + t.id + '">' +
        icon(t.icon) + '<span>' + t.label + '</span></button>'
    ).join('');
  }

  /* ------------------------------------------------------------- readouts */
  function readoutScope() {
    const s = DATA.scope;
    const hot = s.peak_dbfs >= -6;
    return '<div class="eyebrow">输入峰值</div>' +
      '<div class="readout-critical ' + (hot ? 'warn' : '') + '">' + DB(s.peak_dbfs) + '</div>' +
      '<div class="readout-rule"></div>' +
      kv('RMS', DB(s.rms_dbfs), '') + kv('频率', fmt(s.freq_hz, 0) + ' Hz', '') +
      kv('波峰因数', fmt(s.crest, 1) + ' dB', '') + kv('Vpp', fmt(s.vpp, 3) + ' V', '') +
      kv('削波', String(s.clip), s.clip ? 'bad' : '') +
      '<div class="readout-note">快照 · CH' + (s.channel + 1) + ' · 软件不能保护声卡；高压信号必须经过受保护前端。</div>';
  }
  function readoutSpectrum() {
    const s = DATA.spectrum;
    return '<div class="eyebrow">总谐波失真</div>' +
      '<div class="readout-critical ' + (s.thd <= 0.5 ? 'good' : 'warn') + '">' + fmt(s.thd, 3) + '<span class="unit">%</span></div>' +
      '<div class="readout-rule"></div>' +
      kv('THD+N', fmt(s.thdn, 3) + ' %') + kv('SNR', fmt(s.snr, 1) + ' dB') +
      kv('SINAD', fmt(s.sinad, 1) + ' dB') + kv('SFDR', fmt(s.sfdr, 1) + ' dB') +
      kv('ENOB', fmt(s.enob, 1) + ' bit') +
      '<div class="readout-note">Hann 窗 · 8192 点 FFT · 1 kHz 基波；谐波与噪声分开统计。</div>';
  }
  function readoutXY() {
    const s = DATA.xy;
    const p = s.phase_deg;
    return '<div class="eyebrow">相位差</div>' +
      '<div class="readout-critical">' + DEG(p) + '</div>' +
      '<div class="readout-rule"></div>' +
      kv('增益差', minus(s.gain_db, 2) + ' dB') +
      kv('极性', Math.abs(p) > 135 ? '反相' : '正常', Math.abs(p) > 135 ? 'bad' : 'good') +
      kv('频率', fmt(s.freq_hz, 0) + ' Hz') +
      kv('漂移', s.drift_ppm ? fmt(s.drift_ppm, 1) + ' ppm' : '—') +
      '<div class="readout-note">同设备 CH1/CH2 共享时钟；跨设备漂移需要双设备或聚合设备。</div>';
  }
  function readoutMeasure() {
    const pass = DATA.channels.every((c) => c.verdict !== 'FAIL');
    const warn = DATA.channels.some((c) => c.verdict === 'WARN');
    return '<div class="eyebrow">测量判定</div>' +
      '<div class="readout-critical ' + (pass ? (warn ? 'warn' : 'good') : 'bad') + '">' + (pass ? (warn ? 'WARN' : 'PASS') : 'FAIL') + '</div>' +
      '<div class="readout-rule"></div>' +
      kv('通道', String(DATA.channels.length)) + kv('模式', esc(DATA.meta.mode)) +
      kv('采样率', (DATA.meta.rate / 1000).toFixed(1) + ' kHz') +
      kv('限值', '10.0 Vpp') + kv('削波', String(DATA.channels.reduce((a, c) => a + c.clip, 0))) +
      '<div class="readout-note">未校准：只显示 dBFS；电压需要 measured / datasheet / assumed。</div>';
  }
  function readoutRepair() {
    const r = DATA.repair;
    return '<div class="eyebrow">维修计划</div>' +
      '<div class="readout-critical ' + (r.verdict === 'PASS' ? 'good' : 'bad') + '">' + esc(r.verdict) + '</div>' +
      '<div class="readout-rule"></div>' +
      kv('模板', esc(r.label)) + kv('步骤', String(r.steps.length)) +
      kv('通过', String(r.steps.filter((s) => s.pass).length)) +
      kv('来源', '合成验证') +
      '<div class="readout-note">当前只跑合成验证；真实 DUT 闭环需要输出→DUT→输入。</div>';
  }
  function readoutReports() {
    const pass = DATA.reports.filter((r) => r.verdict === 'PASS').length;
    return '<div class="eyebrow">报告</div>' +
      '<div class="readout-critical">' + DATA.reports.length + '<span class="unit">份</span></div>' +
      '<div class="readout-rule"></div>' +
      kv('通过', String(pass)) + kv('注意', String(DATA.reports.filter((r) => r.verdict === 'WARN').length)) +
      kv('失败', String(DATA.reports.filter((r) => r.verdict === 'FAIL').length)) +
      kv('schema', 'v2') +
      '<div class="readout-note">HTML 为 canonical；PDF 为单页可打印摘要；JSON 可机器读取。</div>';
  }
  function readoutCalibration() {
    const c = DATA.calibration;
    return '<div class="eyebrow">校准置信度</div>' +
      '<div class="readout-critical ' + (c.confidence === 'MEASURED' ? 'good' : 'warn') + '">' + esc(c.confidence) + '</div>' +
      '<div class="readout-rule"></div>' +
      kv('Profile', esc(c.id)) + kv('FS Vrms', c.fs_vrms ? fmt(c.fs_vrms, 3) + ' V' : '—') +
      kv('通道', 'CH1') + kv('增益', '0.0 dB') + kv('Pad', '0.0 dB') +
      '<div class="readout-note">' + esc(c.note) + '</div>';
  }
  function kv(k, v, cls) {
    return '<div class="kv"><span class="k">' + k + '</span><span class="v ' + (cls || '') + '">' + v + '</span></div>';
  }

  /* --------------------------------------------------------------- details */
  function detailScope() {
    const s = DATA.scope;
    return '<div class="detail-head"><div><div class="detail-title">信号与电平</div>' +
      '<div class="detail-sub">1 kHz · CH' + (s.channel + 1) + ' · ' + esc(DATA.meta.source) + '</div></div>' +
      '<div class="seg" data-group="timebase"><button class="active">1 s</button><button>200 ms</button><button>50 ms</button></div></div>' +
      '<div class="plot"><canvas id="scopeCanvas"></canvas></div>' +
      '<div class="plot-foot"><div class="legend"><span><i></i>CH1 波形</span><span><i class="soft"></i>触发点</span></div>' +
      '<div class="axis-note">±1.0 FS · 48 kHz · 720 点快照</div></div>';
  }
  function detailSpectrum() {
    return '<div class="detail-head"><div><div class="detail-title">频谱与失真</div>' +
      '<div class="detail-sub">Hann · 8192 FFT · 20 Hz–20 kHz</div></div>' +
      '<div class="seg" data-group="spectrumMode"><button class="' + (state.spectrumMode === 'log' ? 'active' : '') + '" data-mode="log">对数</button><button class="' + (state.spectrumMode === 'linear' ? 'active' : '') + '" data-mode="linear">线性</button></div></div>' +
      '<div class="plot"><canvas id="spectrumCanvas"></canvas></div>' +
      '<div class="plot-foot"><div class="legend"><span><i></i>基波</span><span><i class="soft"></i>谐波</span></div>' +
      '<div class="axis-note">−100…0 dBFS · 1 kHz 基波 · H2/H3 标记</div></div>';
  }
  function detailXY() {
    return '<div class="detail-head"><div><div class="detail-title">XY / Lissajous</div>' +
      '<div class="detail-sub">CH1 → CH2 · 45° · +3.0 dB</div></div>' +
      '<div class="seg" data-group="persistence"><button>关闭</button><button class="active">' + state.persistence + '</button><button>5 s</button></div></div>' +
      '<div class="plot"><canvas id="xyCanvas"></canvas></div>' +
      '<div class="plot-foot"><div class="legend"><span><i></i>实时点</span><span><i class="soft"></i>拟合椭圆</span></div>' +
      '<div class="axis-note">CH1 横轴 · CH2 纵轴 · 归一化到 ±1</div></div>';
  }
  function detailMeasure() {
    const rows = DATA.channels.map((c, i) => {
      const cls = c.verdict === 'FAIL' ? 'bad' : c.verdict === 'WARN' ? 'warn' : c.verdict === 'PASS' ? 'good' : 'dim';
      return '<tr' + (i === 0 ? ' class="sel"' : '') + '><td>' + esc(c.label) + '</td>' +
        '<td class="num">' + minus(c.peak_dbfs, 1) + '</td>' +
        '<td class="num">' + minus(c.rms_dbfs, 1) + '</td>' +
        '<td class="num">' + (c.freq_hz ? fmt(c.freq_hz, 0) : '—') + '</td>' +
        '<td class="num">' + (c.vpp ? fmt(c.vpp, 4) : '—') + '</td>' +
        '<td class="num">' + c.clip + '</td>' +
        '<td class="' + cls + '">' + esc(c.verdict) + '</td></tr>';
    }).join('');
    return '<div class="detail-head"><div><div class="detail-title">通道测量</div>' +
      '<div class="detail-sub">' + esc(DATA.meta.mode) + ' · ' + (DATA.meta.rate / 1000).toFixed(1) + ' kHz · 限值 10.0 Vpp</div></div>' +
      '<div class="seg"><button class="active">全部</button><button>CH1–2</button><button>仅异常</button></div></div>' +
      '<div style="overflow:auto;flex:1 1 auto"><table class="tbl"><thead><tr>' +
      '<th>CH</th><th class="num">Peak dBFS</th><th class="num">RMS dBFS</th><th class="num">Hz</th><th class="num">Vpp</th><th class="num">Clip</th><th>判定</th>' +
      '</tr></thead><tbody>' + rows + '</tbody></table></div>' +
      '<div class="plot-foot"><div class="axis-note">电压列在未校准时显示 —；不伪造精度。</div>' +
      '<div class="axis-note">Repeat 1 · Vpp σ 0.000 · Freq σ 0.00 Hz</div></div>';
  }
  function detailRepair() {
    const steps = DATA.repair.steps.map((s) => {
      const pct = s.unit === 'deg' ? clamp(100 - Math.abs(s.value) / 30 * 100, 8, 100) : clamp(100 - s.value * 100, 8, 100);
      return '<div class="step' + (s.pass ? '' : ' fail') + '"><div class="name">' + esc(s.label) + '</div>' +
        '<div class="val">' + fmt(s.value, 4) + '<span class="u">' + esc(s.unit) + '</span></div>' +
        '<div class="state">' + (s.pass ? 'PASS' : 'FAIL') + '</div>' +
        '<div class="bar" style="--p:' + pct.toFixed(0) + '%"></div></div>';
    }).join('');
    return '<div class="detail-head"><div><div class="detail-title">维修计划 · ' + esc(DATA.repair.label) + '</div>' +
      '<div class="detail-sub">合成验证 · ' + DATA.repair.steps.length + ' 步 · 输出 JSON + PDF</div></div>' +
      '<div class="seg"><button class="active">合成</button><button disabled>Live</button></div></div>' +
      '<div class="steps">' + steps + '</div>' +
      '<div class="plot-foot"><div class="axis-note">阈值：Wow ≤ 0.5% · Flutter ≤ 0.2% · Azimuth ≤ 15° · THD ≤ 0.5%</div>' +
      '<div class="axis-note">真实 DUT 闭环未接线</div></div>';
  }
  function detailReports() {
    const rows = DATA.reports.map((r) => {
      const cls = r.verdict === 'PASS' ? 'good' : r.verdict === 'FAIL' ? 'bad' : 'warn';
      return '<div class="report-row"><div><div class="rname mono">' + esc(r.name) + '</div>' +
        '<div class="rmeta">' + esc(r.kind) + ' · ' + esc(r.time) + ' · JSON / HTML / PDF</div></div>' +
        '<span class="badge ' + cls + '">' + esc(r.verdict) + '</span></div>';
    }).join('');
    return '<div class="detail-head"><div><div class="detail-title">报告与证据</div>' +
      '<div class="detail-sub">canonical HTML · printable PDF · machine-readable JSON</div></div>' +
      '<div class="seg"><button class="active">最近</button><button>按模板</button></div></div>' +
      '<div style="overflow:auto;flex:1 1 auto">' + rows + '</div>' +
      '<div class="plot-foot"><div class="axis-note">报告包含版本、设备、校准 ID、限值、判定与时间戳。</div>' +
      '<div class="axis-note">不含绝对路径 / 用户名</div></div>';
  }
  function detailCalibration() {
    const c = DATA.calibration;
    const rows = [
      ['设备', DATA.meta.device], ['通道', 'CH1'], ['增益', '0.0 dB'], ['Pad', '0.0 dB'],
      ['采样率', (DATA.meta.rate / 1000).toFixed(1) + ' kHz'], ['FS Vrms', c.fs_vrms ? fmt(c.fs_vrms, 3) + ' V' : '—'],
      ['Checksum', c.id === 'none' ? '—' : '已验证'], ['置信度', c.confidence]
    ];
    return '<div class="detail-head"><div><div class="detail-title">校准档案</div>' +
      '<div class="detail-sub">按 device + channel + gain + pad + rate 绑定</div></div>' +
      '<div class="seg"><button class="active">只读</button><button disabled>向导</button></div></div>' +
      '<table class="tbl"><tbody>' + rows.map((r) =>
        '<tr><td class="dim" style="width:38%">' + r[0] + '</td><td class="num">' + esc(r[1]) + '</td></tr>').join('') +
      '</tbody></table>' +
      '<div class="plot-foot"><div class="axis-note">' + esc(c.note) + '</div>' +
      '<div class="axis-note">设备/采样率/增益变化 → 校准失效</div></div>';
  }

  function renderReadout() {
    const map = { scope: readoutScope, spectrum: readoutSpectrum, xy: readoutXY, measure: readoutMeasure,
      repair: readoutRepair, reports: readoutReports, calibration: readoutCalibration };
    $('#readout').innerHTML = (map[state.tab] || readoutScope)();
  }
  function renderDetail() {
    const map = { scope: detailScope, spectrum: detailSpectrum, xy: detailXY, measure: detailMeasure,
      repair: detailRepair, reports: detailReports, calibration: detailCalibration };
    $('#detail').innerHTML = (map[state.tab] || detailScope)();
  }

  /* ---------------------------------------------------------------- params */
  function renderParam() {
    const common = '<div class="param-group"><span class="param-label">输入模式</span><div class="param-value">' + esc(DATA.meta.mode) + '<span class="chev">⌄</span></div></div>' +
      '<div class="param-group"><span class="param-label">采样率</span><div class="param-value">' + (DATA.meta.rate / 1000).toFixed(1) + ' kHz</div></div>' +
      '<div class="param-spacer"></div>';
    let controls = '';
    if (state.tab === 'scope') {
      controls = '<div class="param-group"><span class="param-label">通道</span><div class="param-value">CH1<span class="chev">⌄</span></div></div>' +
        '<div class="param-group"><span class="param-label">量程</span><div class="param-value">±1.0 V<span class="chev">⌄</span></div></div>';
    } else if (state.tab === 'spectrum') {
      controls = '<div class="param-group"><span class="param-label">FFT</span><div class="param-value">8192</div></div>' +
        '<div class="param-group"><span class="param-label">窗</span><div class="param-value">Hann</div></div>';
    } else if (state.tab === 'xy') {
      controls = '<div class="param-group"><span class="param-label">通道</span><div class="param-value">CH1 / CH2</div></div>' +
        '<div class="param-group"><span class="param-label">余晖</span><div class="param-value">' + state.persistence + '</div></div>';
    } else if (state.tab === 'measure') {
      controls = '<div class="param-group"><span class="param-label">通道</span><div class="param-value">全部</div></div>' +
        '<div class="param-group"><span class="param-label">重复</span><div class="param-value">1×</div></div>';
    } else if (state.tab === 'repair') {
      controls = '<div class="param-group"><span class="param-label">模板</span><div class="param-value">' + esc(DATA.repair.template) + '<span class="chev">⌄</span></div></div>' +
        '<div class="param-group"><span class="param-label">来源</span><div class="param-value">合成</div></div>';
    } else if (state.tab === 'reports') {
      controls = '<div class="param-group"><span class="param-label">范围</span><div class="param-value">最近 20<span class="chev">⌄</span></div></div>';
    } else if (state.tab === 'calibration') {
      controls = '<div class="param-group"><span class="param-label">设备</span><div class="param-value">' + esc(DATA.meta.device) + '</div></div>';
    }
    const actions = state.tab === 'repair'
      ? '<button class="btn" data-action="run-repair">运行计划</button><button class="btn primary" data-action="open-pdf">打开 PDF</button>'
      : state.tab === 'reports'
        ? '<button class="btn" data-action="export">导出 JSON</button><button class="btn primary" data-action="open-pdf">打开 PDF</button>'
        : '<button class="btn" data-action="hold">' + (state.held ? '已保持' : '保持') + '</button>' +
          '<button class="btn" data-action="clear">清零</button>' +
          '<button class="btn primary" data-action="export">导出报告</button>';
    $('#param').innerHTML = common + controls + '<div class="param-spacer"></div><div class="param-actions">' + actions + '</div>';
  }

  /* ---------------------------------------------------------------- footer */
  function renderFooter() {
    const m = DATA.meta;
    $('#footer').innerHTML =
      '<span>' + (m.rate / 1000).toFixed(1) + ' kHz · stereo · ' + (m.channels || 2) + ' ch</span>' +
      '<span class="mid">BERRYSTUDIO</span>' +
      '<span class="right"><span><span class="dot"></span>' + esc(m.source) + '</span><span>0.7-alpha</span></span>';
  }

  /* ---------------------------------------------------------------- charts */
  function setupCanvas(canvas) {
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    canvas.width = Math.max(1, Math.round(rect.width * dpr));
    canvas.height = Math.max(1, Math.round(rect.height * dpr));
    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    return { ctx, w: rect.width, h: rect.height };
  }
  function cssVar(name) {
    return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  }
  function grid(ctx, x, y, w, h, cols, rows, color) {
    ctx.save();
    ctx.strokeStyle = color;
    ctx.lineWidth = 1;
    for (let i = 0; i <= cols; i++) {
      const px = Math.round(x + (w * i) / cols) + 0.5;
      ctx.beginPath(); ctx.moveTo(px, y); ctx.lineTo(px, y + h); ctx.stroke();
    }
    for (let i = 0; i <= rows; i++) {
      const py = Math.round(y + (h * i) / rows) + 0.5;
      ctx.beginPath(); ctx.moveTo(x, py); ctx.lineTo(x + w, py); ctx.stroke();
    }
    ctx.restore();
  }
  function label(ctx, text, x, y, color, align) {
    ctx.save();
    ctx.fillStyle = color;
    ctx.font = '9.5px "Geist", "PingFang SC", sans-serif';
    ctx.textAlign = align || 'left';
    ctx.textBaseline = 'middle';
    ctx.fillText(text, x, y);
    ctx.restore();
  }
  function drawScope() {
    const c = $('#scopeCanvas'); if (!c) return;
    const { ctx, w, h } = setupCanvas(c);
    const pad = { l: 36, r: 14, t: 14, b: 26 };
    const pw = w - pad.l - pad.r, ph = h - pad.t - pad.b;
    ctx.clearRect(0, 0, w, h);
    grid(ctx, pad.l, pad.t, pw, ph, 10, 4, cssVar('--plot-grid'));
    ctx.save();
    ctx.strokeStyle = cssVar('--plot-grid-strong');
    ctx.beginPath(); ctx.moveTo(pad.l, pad.t + ph / 2); ctx.lineTo(pad.l + pw, pad.t + ph / 2); ctx.stroke();
    ctx.restore();
    const d = DATA.scope.waveform || [];
    if (d.length > 1) {
      const grad = ctx.createLinearGradient(0, pad.t, 0, pad.t + ph);
      grad.addColorStop(0, cssVar('--plot-fill-top'));
      grad.addColorStop(1, cssVar('--plot-fill-bottom'));
      ctx.beginPath();
      for (let i = 0; i < d.length; i++) {
        const x = pad.l + (i / (d.length - 1)) * pw;
        const y = pad.t + ph / 2 - clamp(d[i] / 1.0, -1, 1) * (ph / 2);
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      const line = ctx;
      line.save();
      line.lineWidth = 1.6;
      line.strokeStyle = cssVar('--plot-curve');
      line.lineJoin = 'round';
      line.stroke();
      line.lineTo(pad.l + pw, pad.t + ph / 2);
      line.lineTo(pad.l, pad.t + ph / 2);
      line.closePath();
      line.fillStyle = grad;
      line.fill();
      line.restore();
    }
    ctx.save();
    ctx.strokeStyle = cssVar('--plot-axis');
    ctx.setLineDash([3, 4]);
    ctx.beginPath(); ctx.moveTo(pad.l + pw * 0.5, pad.t); ctx.lineTo(pad.l + pw * 0.5, pad.t + ph); ctx.stroke();
    ctx.restore();
    label(ctx, '+1.0', pad.l - 8, pad.t + 4, cssVar('--plot-label'), 'right');
    label(ctx, '0', pad.l - 8, pad.t + ph / 2, cssVar('--plot-label'), 'right');
    label(ctx, '−1.0', pad.l - 8, pad.t + ph - 4, cssVar('--plot-label'), 'right');
    label(ctx, '0 ms', pad.l, pad.t + ph + 14, cssVar('--plot-label'), 'left');
    label(ctx, '5 ms', pad.l + pw * 0.25, pad.t + ph + 14, cssVar('--plot-label'), 'center');
    label(ctx, '10 ms', pad.l + pw * 0.5, pad.t + ph + 14, cssVar('--plot-label'), 'center');
    label(ctx, '15 ms', pad.l + pw * 0.75, pad.t + ph + 14, cssVar('--plot-label'), 'center');
  }
  function drawSpectrum() {
    const c = $('#spectrumCanvas'); if (!c) return;
    const { ctx, w, h } = setupCanvas(c);
    const pad = { l: 40, r: 16, t: 14, b: 26 };
    const pw = w - pad.l - pad.r, ph = h - pad.t - pad.b;
    ctx.clearRect(0, 0, w, h);
    grid(ctx, pad.l, pad.t, pw, ph, 6, 5, cssVar('--plot-grid'));
    const s = DATA.spectrum;
    const freqs = s.freqs || [];
    const db = s.db || [];
    const fmin = 20, fmax = 20000;
    const xFor = (f) => pad.l + (Math.log(f / fmin) / Math.log(fmax / fmin)) * pw;
    const yFor = (v) => pad.t + (1 - (clamp(v, -100, 0) + 100) / 100) * ph;
    if (freqs.length > 1) {
      const grad = ctx.createLinearGradient(0, pad.t, 0, pad.t + ph);
      grad.addColorStop(0, cssVar('--plot-fill-top'));
      grad.addColorStop(1, cssVar('--plot-fill-bottom'));
      ctx.beginPath();
      for (let i = 0; i < freqs.length; i++) {
        const x = xFor(freqs[i]);
        const y = yFor(db[i]);
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.save();
      ctx.strokeStyle = cssVar('--spec-curve');
      ctx.lineWidth = 1.25;
      ctx.lineJoin = 'round';
      ctx.stroke();
      ctx.lineTo(pad.l + pw, pad.t + ph);
      ctx.lineTo(pad.l, pad.t + ph);
      ctx.closePath();
      ctx.fillStyle = grad;
      ctx.fill();
      ctx.restore();
      /* harmonic markers */
      const marks = [1000, 2000, 3000, 4000, 5000];
      ctx.save();
      marks.forEach((f) => {
        if (f > fmax) return;
        const x = xFor(f);
        ctx.strokeStyle = cssVar('--plot-axis');
        ctx.globalAlpha = 0.3;
        ctx.beginPath(); ctx.moveTo(x, pad.t); ctx.lineTo(x, pad.t + ph); ctx.stroke();
        ctx.globalAlpha = 1;
        ctx.fillStyle = cssVar('--accent');
        ctx.beginPath(); ctx.arc(x, pad.t + 5, 1.7, 0, 2 * Math.PI); ctx.fill();
      });
      ctx.restore();
    }
    ['0', '−20', '−40', '−60', '−80', '−100'].forEach((t, i) => {
      label(ctx, t, pad.l - 8, pad.t + (ph * i) / 5, cssVar('--plot-label'), 'right');
    });
    ['20', '100', '1k', '10k', '20k'].forEach((t, i) => {
      const f = [20, 100, 1000, 10000, 20000][i];
      label(ctx, t, xFor(f), pad.t + ph + 14, cssVar('--plot-label'), 'center');
    });
  }
  function drawXY() {
    const c = $('#xyCanvas'); if (!c) return;
    const { ctx, w, h } = setupCanvas(c);
    const pad = 18;
    const side = Math.min(w - pad * 2, h - pad * 2);
    const cx = pad + side / 2 + (w - pad * 2 - side) / 2;
    const cy = pad + side / 2 + (h - pad * 2 - side) / 2;
    const r = side / 2;
    ctx.clearRect(0, 0, w, h);
    grid(ctx, cx - r, cy - r, side, side, 8, 8, cssVar('--plot-grid'));
    ctx.save();
    ctx.strokeStyle = cssVar('--plot-grid-strong');
    ctx.beginPath(); ctx.moveTo(cx - r, cy); ctx.lineTo(cx + r, cy);
    ctx.moveTo(cx, cy - r); ctx.lineTo(cx, cy + r); ctx.stroke();
    ctx.restore();
    const pts = DATA.xy.points || [];
    const maxX = Math.max(0.001, ...pts.map((p) => Math.abs(p[0])));
    const maxY = Math.max(0.001, ...pts.map((p) => Math.abs(p[1])));
    ctx.save();
    ctx.strokeStyle = cssVar('--plot-curve');
    ctx.globalAlpha = 0.72;
    ctx.lineWidth = 1.3;
    ctx.lineJoin = 'round';
    ctx.beginPath();
    const cycle = Math.min(pts.length, 240);
    for (let i = 0; i < cycle; i++) {
      const x = cx + (pts[i][0] / maxX) * (r * 0.92);
      const y = cy - (pts[i][1] / maxY) * (r * 0.92);
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.restore();
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = cssVar('--accent');
    for (let i = 0; i < pts.length; i += 2) {
      const x = cx + (pts[i][0] / maxX) * (r * 0.92);
      const y = cy - (pts[i][1] / maxY) * (r * 0.92);
      ctx.globalAlpha = 0.32;
      ctx.fillRect(x - 1, y - 1, 2.2, 2.2);
    }
    ctx.restore();
    /* fitted ellipse */
    ctx.save();
    ctx.strokeStyle = cssVar('--plot-axis');
    ctx.globalAlpha = 0.65;
    ctx.setLineDash([3, 4]);
    ctx.lineWidth = 1;
    const phase = DATA.xy.phase_deg * Math.PI / 180;
    ctx.beginPath();
    for (let i = 0; i <= 240; i++) {
      const t = (i / 240) * Math.PI * 2;
      const x = cx + Math.sin(t) * (r * 0.92);
      const y = cy - Math.sin(t + phase) * (r * 0.92);
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.restore();
    label(ctx, 'CH1 →', cx + r - 2, cy + 14, cssVar('--plot-label'), 'right');
    label(ctx, 'CH2 ↑', cx + 6, cy - r + 2, cssVar('--plot-label'), 'left');
  }
  function drawCharts() {
    if (state.tab === 'scope') drawScope();
    else if (state.tab === 'spectrum') drawSpectrum();
    else if (state.tab === 'xy') drawXY();
  }

  /* ---------------------------------------------------------------- modals */
  function modalHTML(kind) {
    const m = DATA.meta;
    if (kind === 'about') {
      return '<div class="modal" role="dialog" aria-modal="true" aria-label="关于 AudioScope">' +
        mark(64) +
        '<h2>AudioScope</h2><div class="sub">一个 BerryStudio 产品 · ' + esc(m.version) + '</div>' +
        '<p>基于声卡的音频维修工作台。安全预检、校准、测量、维修计划与报告，全部本地运行。</p>' +
        '<div class="rule"></div>' +
        '<div class="meta-grid">' +
          '<div class="kv"><span class="k">平台</span><span class="v">' + esc(m.platform) + '</span></div>' +
          '<div class="kv"><span class="k">测量标准</span><span class="v">dBFS · Vpp · THD</span></div>' +
          '<div class="kv"><span class="k">采样率</span><span class="v">' + (m.rate / 1000).toFixed(1) + ' kHz</span></div>' +
          '<div class="kv"><span class="k">通道</span><span class="v">' + (m.channels || 2) + '</span></div>' +
          '<div class="kv"><span class="k">报告</span><span class="v">JSON · HTML · PDF</span></div>' +
          '<div class="kv"><span class="k">许可证</span><span class="v">Apache-2.0</span></div>' +
        '</div>' +
        '<p class="note">软件不能保护声卡。高压/未知信号必须经过受保护前端；不要接市电、喇叭输出或未衰减胆机输出。</p>' +
        '<div class="modal-foot"><span class="brandline">BERRYSTUDIO / AUDIOSCOPE</span><button class="btn" data-close>关闭</button></div>' +
      '</div>';
    }
    if (kind === 'settings') {
      return '<div class="modal wide" role="dialog" aria-modal="true" aria-label="设置">' +
        '<h2>设置</h2><div class="sub">界面缩放 · 主题 · 快捷键 · ' + esc(m.version) + '</div>' +
        '<div class="rule"></div>' +
        '<div class="eyebrow">界面缩放</div><div class="zoom-row">' +
          ['80%', '100%', '125%', '150%', '200%'].map((z) => '<button class="btn' + (z === '100%' ? ' primary' : '') + '">' + z + '</button>').join('') +
        '</div>' +
        '<p class="note">缩放整个 1120×700 表面，不单独放大文字；与 MasterLens 基线一致。</p>' +
        '<div class="rule"></div>' +
        '<div class="meta-grid">' +
          '<div class="kv"><span class="k">主题</span><span class="v">' + (state.theme === 'dark' ? '深色' : '浅色') + '</span></div>' +
          '<div class="kv"><span class="k">导航</span><span class="v"><span class="kbd">1</span>–<span class="kbd">7</span></span></div>' +
          '<div class="kv"><span class="k">导出</span><span class="v"><span class="kbd">⌘E</span></span></div>' +
          '<div class="kv"><span class="k">关闭弹窗</span><span class="v"><span class="kbd">Esc</span></span></div>' +
        '</div>' +
        '<div class="rule"></div>' +
        '<p class="note">AudioScope 只读本地测量；无网络、无遥测。电压在未校准时显示 —，不伪造精度。</p>' +
        '<div class="modal-foot"><span class="brandline">BERRYSTUDIO / AUDIOSCOPE</span><button class="btn" data-close>关闭</button></div>' +
      '</div>';
    }
    return '';
  }
  function openModal(kind) {
    state.modal = kind;
    const root = $('#modal-root');
    root.innerHTML = modalHTML(kind);
    root.classList.add('open');
    const close = $('[data-close]', root);
    if (close) close.focus();
  }
  function closeModal() {
    const root = $('#modal-root');
    if (!root.classList.contains('open')) return;
    root.classList.remove('open');
    root.innerHTML = '';
    state.modal = null;
  }

  /* ---------------------------------------------------------------- actions */
  function toast(text) {
    const el = $('#toast');
    el.textContent = text;
    el.classList.add('show');
    clearTimeout(toast._t);
    toast._t = setTimeout(() => el.classList.remove('show'), 1800);
  }
  function exportJSON() {
    const blob = new Blob([JSON.stringify(DATA, null, 2)], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'audioscope-snapshot.json';
    a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 1000);
    toast('已导出 snapshot JSON');
  }
  function setTab(id, push) {
    if (!TABS.some((t) => t.id === id)) return;
    state.tab = id;
    if (push !== false) history.replaceState(null, '', '#' + id);
    renderRail();
    renderReadout();
    renderDetail();
    renderParam();
    drawCharts();
  }
  function toggleTheme() {
    state.theme = state.theme === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', state.theme);
    localStorage.setItem('audioscope-theme', state.theme);
    renderHeader();
    drawCharts();
  }

  /* ---------------------------------------------------------------- wiring */
  document.addEventListener('click', (e) => {
    const tab = e.target.closest('[data-tab]');
    if (tab) { setTab(tab.dataset.tab); return; }
    const modal = e.target.closest('[data-modal]');
    if (modal) { openModal(modal.dataset.modal); return; }
    if (e.target.closest('[data-close]') || e.target.id === 'modal-root') { closeModal(); return; }
    const action = e.target.closest('[data-action]');
    if (!action) return;
    const name = action.dataset.action;
    if (name === 'theme') toggleTheme();
    else if (name === 'snapshot') { toast('快照数据 · ' + DATA.meta.source); }
    else if (name === 'export') exportJSON();
    else if (name === 'hold') { state.held = !state.held; renderParam(); toast(state.held ? '已保持当前快照' : '已恢复实时'); }
    else if (name === 'clear') { state.held = false; renderParam(); toast('显示已清零（演示）'); }
    else if (name === 'run-repair') { toast('合成维修计划已完成 · ' + DATA.repair.verdict); }
    else if (name === 'open-pdf') { toast('PDF 位于报告输出目录'); }
  });
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') { closeModal(); return; }
    if (state.modal) return;
    if (e.key >= '1' && e.key <= '7') {
      const idx = Number(e.key) - 1;
      if (TABS[idx]) setTab(TABS[idx].id);
    }
    if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === 'e') {
      e.preventDefault(); exportJSON();
    }
  });
  window.addEventListener('hashchange', () => {
    const id = (location.hash || '#scope').slice(1);
    if (id === 'about' || id === 'settings') { openModal(id); return; }
    closeModal();
    if (id !== state.tab) setTab(id, false);
  });
  window.addEventListener('resize', () => {
    applyScale();
    drawCharts();
  });

  function applyScale() {
    const scale = Math.min(window.innerWidth / 1120, window.innerHeight / 700, 1);
    document.documentElement.style.setProperty('--scale', scale.toFixed(4));
  }

  /* ----------------------------------------------------------------- boot */
  renderHeader();
  renderRail();
  renderReadout();
  renderDetail();
  renderParam();
  renderFooter();
  applyScale();
  requestAnimationFrame(drawCharts);
  if (initialModal) requestAnimationFrame(() => openModal(initialModal));
})();
