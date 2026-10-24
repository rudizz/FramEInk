"use strict";
// Real portal script, in-memory DOM/viewport/SD API. No actual user files.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

class Element {
  constructor(tag = "div") {
    Object.assign(this, {tag, children: [], listeners: {}, textContent: "", mockVisible: false, classList: {toggle() {}}});
  }
  addEventListener(name, handler) { this.listeners[name] = handler; }
  setAttribute(name, value) { this[name] = value; }
  removeAttribute(name) { delete this[name]; }
  showModal() { this.open = true; }
  getContext() {
    if (!this.context) {
      const calls = [];
      this.context = {calls, fillRect(...args) { calls.push(["fillRect", ...args]); },
        drawImage(...args) { calls.push(["drawImage", ...args]); },
        save() {}, restore() {}, translate(...args) { calls.push(["translate", ...args]); },
        rotate(...args) { calls.push(["rotate", ...args]); },
        getImageData(...args) { calls.push(["getImageData", ...args]); return {data: new Uint8ClampedArray([255, 0, 0, 255])}; },
        putImageData(...args) { calls.push(["putImageData", ...args]); }};
    }
    return this.context;
  }
  toBlob(callback, type) { callback({thumbnail: true, type, size: 100}); }
  append(...children) { this.children.push(...children); }
  appendChild(child) { this.append(child); }
  replaceChildren() { this.children = []; }
  getBoundingClientRect() {
    return {top: this.mockVisible ? 120 : 2000, bottom: this.mockVisible ? 220 : 2100, left: 20, right: 120};
  }
  querySelectorAll(selector) {
    const all = this.children.flatMap(child => [child, ...child.querySelectorAll("*")]);
    if (selector === "*") return all;
    return all.filter(child => selector === "input:checked" ? child.tag === "input" && child.checked : selector.split(",").includes(child.tag));
  }
}

async function harness({withObserver = true, settings = {ssid: "Test WiFi"},
  target = {width: 600, height: 448, sdReady: true, directory: "/landscape", maxPhotoBytes: 2 * 1024 * 1024}} = {}) {
  const elements = new Map();
  const byId = id => {
    if (!elements.has(id)) elements.set(id, new Element());
    return elements.get(id);
  };
  const state = {photos: Array.from({length: 23}, (_, i) => ({id: "id" + i, name: "photo" + i + ".jpg", size: 1024,
      modified: new Date(Date.UTC(2026, 9, 12, 12) - i * 2000).toISOString().slice(0, 19)})),
    offsets: [], images: [], aborted: [], deleted: [], confirm: false, confirmations: [], failOffset: null,
    frames: [], endVisibleAt: null, deferred: new Set(), revoked: [], observer: null, savedSettings: [], listQueries: [], uploads: [],
    uploadRequests: [], deferUploads: false, deferPaint: false};
  const documentListeners = {};
  const response = (body, ok = true) => ({ok, json: async () => body, text: async () => String(body), blob: async () => body});
  const document = {hidden: false, getElementById: byId, createElement: tag => new Element(tag),
    addEventListener: (event, handler) => { documentListeners[event] = handler; }};
  class Observer {
    constructor(callback, options) {
      assert.equal(options.root, null); // Respect the outer page viewport too.
      assert.equal(options.rootMargin, "0px"); // No off-screen prefetch.
      this.callback = callback;
      this.targets = new Set();
      state.observer = this;
    }
    observe(target) { this.targets.add(target); }
    unobserve(target) { this.targets.delete(target); }
  }
  const context = vm.createContext({
    document, window: {innerHeight: 800, innerWidth: 600, addEventListener() {},
      confirm: text => { state.confirmations.push(text); return state.confirm; }},
    IntersectionObserver: withObserver ? Observer : undefined, URLSearchParams, AbortController,
    FormData: class { constructor(form) { this.values = form ? form.values : {}; }
      append(key, value, name) { this.values[key] = {value, name}; }
      [Symbol.iterator]() { return Object.entries(this.values)[Symbol.iterator](); } },
    XMLHttpRequest: class {
      constructor() { this.upload = {}; this.headers = {}; }
      open(method, url) { assert.equal(method, "POST"); assert.equal(url, "/api/photos"); }
      setRequestHeader(key, value) { this.headers[key] = value; }
      send(body) { state.uploads.push({headers: this.headers, body: body.values});
        state.uploadRequests.push(this);
        if (!state.deferUploads) this.complete(); }
      complete(status = 201, text = '{"path":"/landscape/test.jpg"}') {
        this.status = status; this.responseText = text; this.onload(); }
    },
    URL: {createObjectURL: () => "blob:thumbnail-" + state.images.length, revokeObjectURL: url => state.revoked.push(url)},
    requestAnimationFrame: callback => { state.frames.push(callback); return state.frames.length; },
    setInterval: () => 1, clearInterval() {},
    setTimeout: (callback, delay) => delay === 0 || (delay === 150 && !state.deferPaint) ? setImmediate(callback) : 1,
    clearTimeout() {},
    fetch: async (url, options = {}) => {
      if (url === "/api/device") return response(target);
      if (url === "/api/settings") {
        if (options.method === "POST") {
          state.savedSettings.push(Object.fromEntries(options.body));
          return response("Settings saved.");
        }
        return response(settings);
      }
      if (url === "/api/ping") return response("");
      if (url.startsWith("/api/photos?")) {
        const params = new URLSearchParams(url.split("?")[1]);
        const offset = Number(params.get("offset"));
        state.offsets.push(offset);
        state.listQueries.push(Object.fromEntries(params));
        if (offset === state.failOffset) return response("Simulated listing failure", false);
        const sorted = [...state.photos].sort((a, b) => {
          let comparison = 0;
          if (params.get("sort") !== "name") comparison = (a.modified || "").localeCompare(b.modified || "");
          comparison ||= a.name.toLowerCase().localeCompare(b.name.toLowerCase()) || a.name.localeCompare(b.name);
          return params.get("order") === "asc" ? comparison : -comparison;
        });
        const start = params.has("cursor") ? sorted.findIndex(photo => photo.id === params.get("cursor")) + 1 : offset;
        const page = sorted.slice(start, start + 10);
        const more = start + 10 < sorted.length;
        return response({photos: page, nextOffset: more ? offset + page.length : null,
          nextCursor: more ? page.at(-1).id : null, totalPhotos: state.photos.length,
          totalPages: Math.ceil(state.photos.length / 10), pageSize: 10});
      }
      if (url.startsWith("/api/photo?")) {
        const id = url.split("=")[1];
        state.images.push(id);
        if (state.deferred.has(id)) return new Promise((resolve, reject) => {
          options.signal.addEventListener("abort", () => { state.aborted.push(id); reject(new Error("Aborted")); }, {once: true});
        });
        return response({id});
      }
      if (url === "/api/photos/delete") {
        const id = options.body.get("id");
        assert.equal(options.headers["X-FramEInk-Request"], "1");
        state.deleted.push(id);
        state.photos = state.photos.filter(photo => photo.id !== id);
        return response("Photo deleted.");
      }
      throw new Error("Unexpected request: " + url);
    }
  });
  const jobs = () => vm.runInContext("galleryPreviews", context);
  byId("gallery-viewport").getBoundingClientRect = () => ({top: 100, bottom: 600, left: 0, right: 580});
  byId("gallery-end").getBoundingClientRect = () => ({top: jobs().length === state.endVisibleAt ? 500 : 2000,
    bottom: jobs().length === state.endVisibleAt ? 550 : 2050, left: 20, right: 550});
  byId("photo-transfer").hidden = byId("progress").hidden = byId("editor").hidden = true;
  vm.runInContext(fs.readFileSync(path.join(__dirname, "../FramEink.Core/web/portal.js"), "utf8"), context);
  // Mock only decoding. Actual downloading, resize, cancellation and URL lifecycle run.
  vm.runInContext("loadImage = async () => ({naturalWidth: 600, naturalHeight: 448});", context);
  await new Promise(resolve => setImmediate(resolve));
  const click = id => byId(id).listeners.click();
  const boxes = () => byId("gallery").querySelectorAll("input");
  const count = () => vm.runInContext("gallerySelection.size", context);
  const check = (index, checked) => { const box = boxes()[index]; box.checked = checked; box.listeners.change(); };
  const pump = async () => {
    for (let turn = 0; turn < 20; turn++) {
      for (const frame of state.frames.splice(0)) frame();
      await new Promise(resolve => setImmediate(resolve));
      if (!state.frames.length) return;
    }
    throw new Error("Gallery scheduler did not settle");
  };
  const visible = ids => {
    for (const job of jobs()) job.image.mockVisible = ids.includes(job.photo.id);
    if (state.observer) state.observer.callback(Array.from(state.observer.targets, target => ({target,
      isIntersecting: target.mockVisible, intersectionRatio: target.mockVisible ? 1 : 0})));
    byId("gallery-viewport").listeners.scroll();
  };
  const append = async () => {
    state.endVisibleAt = jobs().length;
    visible([]);
    await pump();
    state.endVisibleAt = null;
  };
  return {state, byId, click, boxes, count, check, pump, visible, append, jobs, context, document, documentListeners};
}

async function run() {
  await editorTests();
  await photoProgressTests();
  await gallerySortingTests();
  const lazy = await harness();
  assert.equal(lazy.byId("nightPauseStart").value, "23:00");
  assert.equal(lazy.byId("nightPauseEnd").value, "05:00");
  assert.equal(lazy.byId("updateMinute").value, "00");
  assert.equal(lazy.byId("photoOrder").value, "random");
  const settings = await harness({settings: {ssid: "Test WiFi", nightPauseStart: "22:30", nightPauseEnd: "06:15", updateMinute: "45", photoOrder: "alphabetical"}});
  assert.equal(settings.byId("nightPauseStart").value, "22:30");
  assert.equal(settings.byId("nightPauseEnd").value, "06:15");
  assert.equal(settings.byId("updateMinute").value, "45");
  assert.equal(settings.byId("photoOrder").value, "alphabetical");
  const save = async (start, end, minute = "45", order = "alphabetical") => settings.byId("settings-form").listeners.submit({preventDefault() {}, target: {
    values: {ssid: "Test WiFi", password: "", latitude: "48.1", longitude: "11.5", calendarUrl: "", nightPauseStart: start, nightPauseEnd: end, updateMinute: minute, photoOrder: order}
  }});
  await save("22:30", "06:15");
  assert.equal(settings.state.savedSettings.length, 1);
  assert.equal(settings.state.savedSettings[0].nightPauseStart, "22:30");
  assert.equal(settings.state.savedSettings[0].nightPauseEnd, "06:15");
  assert.equal(settings.state.savedSettings[0].updateMinute, "45");
  assert.equal(settings.state.savedSettings[0].photoOrder, "alphabetical");
  for (const [start, end] of [["24:00", "05:00"], ["23:00", "06:60"], ["00:00", "23:01"], ["", "05:00"]]) {
    await save(start, end);
    assert.equal(settings.state.savedSettings.length, 1);
    assert(settings.byId("settings-status").textContent.length > 0);
  }
  await save("23:00", "23:00"); // Equal times disable the pause.
  assert.equal(settings.state.savedSettings.length, 2);
  for (const minute of ["", "60", "99", "100", "-1", "1.5", "1e1", " 5", "5 ", "+1", "00:00"]) {
    await save("23:00", "05:00", minute);
    assert.equal(settings.state.savedSettings.length, 2);
    assert(settings.byId("settings-status").textContent.includes("00 to 59"));
  }
  for (const minute of ["00", "0", "5", "05", "59"]) {
    await save("23:00", "05:00", minute);
    assert.equal(settings.state.savedSettings.at(-1).updateMinute, minute);
  }
  const midnightSettings = await harness({settings: {ssid: "Test WiFi", updateMinute: 0}});
  assert.equal(midnightSettings.byId("updateMinute").value, "00");
  let saves = settings.state.savedSettings.length;
  for (const order of ["", "Random", "ALPHABETICAL", "0", "1", "date", " random", "random "]) {
    await save("23:00", "05:00", "00", order);
    assert.equal(settings.state.savedSettings.length, saves);
    assert(settings.byId("settings-status").textContent.includes("photo order"));
  }
  for (const order of ["random", "alphabetical"]) {
    await save("23:00", "05:00", "00", order);
    assert.equal(settings.state.savedSettings.length, ++saves);
    assert.equal(settings.state.savedSettings.at(-1).photoOrder, order);
  }
  await lazy.pump();
  assert.deepEqual(lazy.state.offsets, [0]);
  assert.equal(lazy.boxes().length, 10);
  assert.deepEqual(lazy.state.images, []); // Off-screen gallery downloads nothing.
  lazy.visible(["id0", "id1"]);
  await lazy.pump();
  assert.deepEqual(lazy.state.images, ["id0", "id1"]);
  assert(lazy.jobs().slice(0, 2).every(job => job.loaded && job.image.src.startsWith("blob:")));
  lazy.visible(["id2", "id3"]);
  await lazy.pump();
  assert.deepEqual(lazy.state.images, ["id0", "id1", "id2", "id3"]);
  lazy.visible(["id0", "id1"]);
  await lazy.pump();
  assert.equal(lazy.state.images.length, 4); // Scrolling back uses cached thumbnails.
  lazy.state.deferred.add("id4");
  lazy.visible(["id4"]);
  await lazy.pump();
  lazy.visible(["id5"]);
  await lazy.pump();
  assert.deepEqual(lazy.state.aborted, ["id4"]);
  assert.equal(lazy.jobs()[4].loaded, false);
  assert.equal(lazy.jobs()[5].loaded, true);
  lazy.state.deferred.delete("id4");
  lazy.visible(["id4"]);
  await lazy.pump();
  assert.equal(lazy.jobs()[4].loaded, true);
  lazy.document.hidden = true;
  lazy.documentListeners.visibilitychange();
  lazy.visible(["id6"]);
  await lazy.pump();
  assert(!lazy.state.images.includes("id6"));
  lazy.document.hidden = false;
  lazy.documentListeners.visibilitychange();
  await lazy.pump();
  assert(lazy.state.images.includes("id6"));
  lazy.check(0, true);
  lazy.byId("gallery-viewport").scrollTop = 900;
  lazy.state.failOffset = 10;
  await lazy.append();
  assert.equal(lazy.boxes().length, 10);
  assert.equal(lazy.byId("retry-photos").hidden, false);
  assert.equal(lazy.state.offsets.filter(offset => offset === 10).length, 1); // No retry loop.
  lazy.state.failOffset = null;
  await lazy.click("retry-photos");
  await lazy.pump();
  assert.equal(lazy.boxes().length, 20);
  assert.equal(lazy.byId("gallery-viewport").scrollTop, 900);
  assert(lazy.boxes()[0].checked);
  assert(lazy.jobs()[0].loaded);
  await lazy.append();
  assert.equal(lazy.boxes().length, 23);
  assert.equal(lazy.byId("gallery-count").textContent, "23 of 23 photo(s) listed.");
  assert.equal(lazy.byId("gallery-end-status").textContent, "All photos are listed.");
  await lazy.click("refresh-photos");
  assert.equal(lazy.count(), 0);
  assert.equal(lazy.boxes().length, 10);
  assert.equal(lazy.byId("gallery-viewport").scrollTop, 0);
  assert.equal(lazy.state.revoked.length, 7);

  const h = await harness();
  h.state.offsets = [];
  await h.click("select-all-photos");
  assert.deepEqual(h.state.offsets, [0, 10, 20]);
  assert.equal(h.count(), 23);
  assert.equal(h.boxes().length, 10); // Select all fetches names, not images/cards.
  assert.deepEqual(h.state.images, []);
  assert(h.boxes().every(box => box.checked));
  assert.equal(h.byId("delete-photos").textContent, "Delete selected (23)");
  assert.equal(h.byId("gallery-selection").textContent, "23 photo(s) selected.");
  await h.append();
  assert(h.boxes().every(box => box.checked));
  h.check(0, false);
  assert.equal(h.count(), 22);
  await h.click("select-photos");
  assert.equal(h.count(), 23);
  await h.click("select-photos"); // Clear only the 20 listed items.
  assert.equal(h.count(), 3);
  assert(h.boxes().every(box => !box.checked));
  await h.click("clear-photos");
  assert.equal(h.count(), 0);
  assert.equal(h.byId("gallery-selection").hidden, true);
  h.check(0, true);
  h.check(10, true);
  await h.click("delete-photos"); // Cancellation preserves selection and files.
  assert.equal(h.state.deleted.length, 0);
  assert.equal(h.count(), 2);
  h.state.confirm = true;
  await h.click("delete-photos");
  assert.deepEqual(h.state.deleted, ["id0", "id10"]);
  assert.equal(h.count(), 0);
  assert.equal(h.state.photos.length, 21);
  await h.click("select-all-photos");
  await h.click("delete-photos"); // In-memory fixtures only.
  assert.equal(h.state.photos.length, 0);
  assert.equal(h.count(), 0);
  assert.equal(h.byId("select-all-photos").disabled, true);
  assert.equal(h.byId("gallery-count").hidden, true);
  assert.match(h.state.confirmations.at(-1), /21 selected photo/);
  assert.match(h.state.confirmations.at(-1), /and 1 more/);
  const failure = await harness();
  failure.check(0, true);
  failure.state.failOffset = 10;
  await failure.click("select-all-photos");
  assert.equal(failure.count(), 1);
  assert(failure.boxes()[0].checked);
  assert.match(failure.byId("gallery-status").textContent, /previous selection has been kept/);
  const fallback = await harness({withObserver: false});
  fallback.visible(["id0"]);
  await fallback.pump();
  assert.deepEqual(fallback.state.images, ["id0"]);
  console.log("Portal gallery: lazy visibility, infinite scroll, cache, cancellation, retry, selection and deletion passed.");
}

async function gallerySortingTests() {
  const gallery = await harness();
  assert.equal(gallery.byId("gallery-sort").value, "modified");
  assert.equal(gallery.byId("gallery-order").value, "desc");
  assert.deepEqual(gallery.state.images, []);
  assert.deepEqual(gallery.state.listQueries[0], {offset: "0", sort: "modified", order: "desc"});
  gallery.check(0, true);
  for (const sort of ["name", "modified"]) for (const order of ["asc", "desc"]) {
    gallery.byId("gallery-sort").value = sort;
    gallery.byId("gallery-order").value = order;
    await gallery.byId("gallery-sort").listeners.change();
    assert.equal(gallery.byId("gallery-viewport").scrollTop, 0);
    assert.equal(gallery.count(), 1); // Reordering doesn't lose explicit selections.
    const query = gallery.state.listQueries.at(-1);
    assert.deepEqual(query, {offset: "0", sort, order});
    const expected = [...gallery.state.photos].sort((a, b) => {
      const comparison = sort === "modified" ? a.modified.localeCompare(b.modified) : a.name.localeCompare(b.name);
      return order === "asc" ? comparison : -comparison;
    }).map(photo => photo.id);
    assert.deepEqual(Array.from(gallery.jobs(), job => job.photo.id), expected.slice(0, 10));
    await gallery.append();
    assert.deepEqual(Array.from(gallery.jobs(), job => job.photo.id), expected.slice(0, 20));
    assert.equal(gallery.state.listQueries.at(-1).cursor, expected[9]);
    await gallery.append();
    assert.deepEqual(Array.from(gallery.jobs(), job => job.photo.id), expected);
  }
  assert.deepEqual(gallery.state.images, []); // Sorting downloads names/dates, never off-screen images.
  await gallery.click("select-all-photos");
  assert.equal(gallery.count(), 23);
  const batches = gallery.state.listQueries.slice(-3);
  assert(batches.every(query => query.sort === "modified" && query.order === "desc"));
  assert(batches[1].cursor && batches[2].cursor);
}

async function editorTests() {
  for (const target of [{width: 600, height: 448, grayscale: false, sdReady: true},
    {width: 825, height: 1200, grayscale: true, sdReady: true}]) {
    target.maxPhotoBytes = 2 * 1024 * 1024;
    const editor = await harness({target});
    const preview = editor.byId("preview");
    const calls = preview.getContext().calls;
    const close = (actual, expected) => assert(Math.abs(actual - expected) < 1e-8, actual + " != " + expected);
    vm.runInContext("sourceImage = {naturalWidth: 2000, naturalHeight: 1000};", editor.context);
    for (const fit of ["contain", "cover"]) for (const zoom of [50, 100, 175, 400])
      for (const rotation of [0, 90, 180, 270]) for (const pan of [0, 50, 100]) {
        editor.byId("fit").value = fit;
        editor.byId("zoom").value = String(zoom);
        editor.byId("crop-x").value = editor.byId("crop-y").value = String(pan);
        calls.length = 0;
        vm.runInContext("rotation = " + rotation + "; drawPreview();", editor.context);
        assert.equal(preview.width, target.width);
        assert.equal(preview.height, target.height);
        assert.equal(editor.byId("zoom-value").textContent, zoom + "%");
        const draw = calls.find(call => call[0] === "drawImage");
        const translate = calls.find(call => call[0] === "translate");
        const width = rotation % 180 ? draw[5] : draw[4];
        const height = rotation % 180 ? draw[4] : draw[5];
        close(translate[1] - width / 2, (target.width - width) * pan / 100);
        close(translate[2] - height / 2, (target.height - height) * pan / 100);
        close(draw[4] / draw[5], 2); // No distortion, even after rotation/zoom.
        close(calls.find(call => call[0] === "rotate")[1], rotation * Math.PI / 180);
        if (zoom === 100) {
          if (fit === "contain") assert(width <= target.width + 1e-8 && height <= target.height + 1e-8);
          else assert(width >= target.width - 1e-8 && height >= target.height - 1e-8);
        }
        // The grayscale property describes the physical display. Both SD JPEGs
        // and previews keep the source colors, including on Inkplate 10.
        assert.equal(calls.some(call => call[0] === "getImageData" || call[0] === "putImageData"), false);
      }
    // A new queued photo resets zoom/pan; export is the same target-sized canvas.
    await editor.byId("files").listeners.change({target: {files: [{name: "phone.jpg", size: 100}], value: ""}});
    assert.equal(editor.byId("zoom").value, "100");
    assert.equal(editor.byId("crop-x").value, "50");
    assert.equal(editor.byId("crop-y").value, "50");
    editor.byId("zoom").value = "225";
    editor.byId("zoom").listeners.input();
    assert.equal(editor.byId("zoom-value").textContent, "225%");
    await editor.click("upload");
    assert.equal(editor.state.uploads.length, 1);
    assert.equal(editor.state.uploads[0].body.photo.value.type, "image/jpeg");
    assert.match(editor.state.uploads[0].headers["X-FramEInk-Upload-Time"], /^\d{17}$/);
  }
}

async function photoProgressTests() {
  const editor = await harness({target: {name: "FramEInk 10", width: 825, height: 1200,
    grayscale: true, sdReady: true, directory: "/portrait", maxPhotoBytes: 2 * 1024 * 1024}});
  const state = expression => vm.runInContext(expression, editor.context);
  const tick = () => new Promise(resolve => setImmediate(resolve));
  const panel = editor.byId("photo-transfer");
  const progress = editor.byId("progress");
  const title = () => editor.byId("transfer-title").textContent;
  const detail = () => editor.byId("transfer-detail").textContent;
  assert.match(editor.byId("device").textContent, /grayscale display · color photos on SD/);
  // Opening the source menu/library must not show progress, shift the page,
  // disable the input or prevent the native picker's default action.
  let prevented = false;
  const nativeClick = {preventDefault() { prevented = true; }};
  editor.byId("pick-label").listeners.pointerdown?.(nativeClick);
  editor.byId("files").listeners.click?.(nativeClick);
  assert.equal(panel.hidden, true);
  assert.equal(editor.byId("files").disabled, false);
  assert.equal(state("busy"), false);
  assert.equal(prevented, false);
  editor.byId("files").listeners.cancel?.();
  assert.equal(panel.hidden, true);
  assert.equal(state("queue.length"), 0);

  let finishDecode;
  editor.context.testDecode = () => new Promise(resolve => { finishDecode = resolve; });
  vm.runInContext("loadImage = testDecode;", editor.context);
  editor.state.deferPaint = true;
  const selection = editor.byId("files").listeners.change({target: {
    files: [{name: "first.jpg", size: 100}, {name: "second.jpg", size: 100}], value: ""}});
  assert.equal(panel.hidden, false);
  assert.match(title(), /Preparing photo 1 of 2/);
  assert.equal(progress.value, undefined);
  assert.equal(editor.byId("finish").disabled, true);
  assert.equal(editor.byId("editor").hidden, true);
  await tick();
  assert.equal(finishDecode, undefined); // Preparation message paints first.
  await editor.pump();
  assert(finishDecode);
  finishDecode({naturalWidth: 2000, naturalHeight: 1000});
  await selection;
  assert.equal(panel.hidden, true);
  assert.equal(editor.byId("editor").hidden, false);
  assert.equal(editor.byId("upload").disabled, false);
  editor.state.deferPaint = false;
  vm.runInContext("loadImage = async () => ({naturalWidth: 600, naturalHeight: 448});", editor.context);

  // Cancelling a new picker must preserve the already-prepared queue/photo.
  editor.byId("files").listeners.click?.(nativeClick);
  editor.byId("files").listeners.cancel?.();
  await editor.byId("files").listeners.change({target: {files: [], value: ""}});
  assert.equal(state("queue.length"), 2);
  assert(state("sourceImage !== null"));

  editor.state.deferUploads = true;
  const transfer = editor.click("upload");
  assert.equal(panel.hidden, false);
  assert.match(title(), /Preparing photo 1 of 2 for transfer/);
  await tick();
  const xhr = editor.state.uploadRequests[0];
  assert(xhr);
  assert.equal(progress.value, 0);
  assert.equal(editor.byId("files").disabled, true);
  xhr.upload.onprogress({lengthComputable: true, loaded: 1024, total: 4096});
  assert.equal(progress.value, 25);
  assert.equal(editor.byId("transfer-percent").textContent, "25%");
  assert.match(detail(), /1 KB of 4 KB/);
  xhr.upload.onprogress({lengthComputable: false, loaded: 2048});
  assert.equal(progress.value, undefined);
  assert.match(detail(), /2 KB sent/);
  xhr.upload.onprogress({lengthComputable: true, loaded: 4096, total: 4096});
  xhr.upload.onload();
  assert.match(title(), /Saving photo 1 of 2/);
  assert.match(detail(), /Waiting for the frame to confirm/);
  assert.equal(progress.value, 100);
  assert.equal(state("savedCount"), 0); // Sent bytes are not yet a confirmed save.
  await editor.click("upload");
  assert.equal(editor.state.uploadRequests.length, 1); // No double submission.
  xhr.complete();
  await transfer;
  assert.equal(state("savedCount"), 1);
  assert.equal(state("currentIndex"), 1);

  // A timeout retains the current photo, shows a stable error, and allows retry.
  const timeout = editor.click("upload");
  await tick();
  editor.state.uploadRequests[1].ontimeout();
  await timeout;
  assert.match(title(), /not confirmed/);
  assert.match(detail(), /timed out/);
  assert.equal(progress.hidden, true);
  assert.equal(panel.hidden, false);
  assert.equal(state("savedCount"), 1);
  assert.equal(state("currentIndex"), 1);
  assert.equal(editor.byId("upload").disabled, false);
  editor.state.deferUploads = false;
  await editor.click("upload");
  assert.equal(state("savedCount"), 2);
  assert.match(title(), /Photos saved to the SD card/);
  assert.equal(progress.value, 100);
  assert.equal(panel.hidden, false);
  assert.equal(editor.byId("dismiss-progress").hidden, false);
  editor.click("dismiss-progress");
  assert.equal(panel.hidden, true);
  console.log("Portal uploads: picker/decoding, byte progress, SD acknowledgement, timeout/retry, queue and original colors passed.");
}
module.exports = {run};
if (require.main === module) run().catch(error => { console.error(error); process.exitCode = 1; });
