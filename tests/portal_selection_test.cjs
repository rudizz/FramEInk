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
  setAttribute() {}
  removeAttribute(name) { delete this[name]; }
  showModal() { this.open = true; }
  getContext() { return {fillRect() {}, drawImage() {}}; }
  toBlob(callback) { callback({thumbnail: true}); }
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

async function harness({withObserver = true, settings = {ssid: "Test WiFi"}} = {}) {
  const elements = new Map();
  const byId = id => {
    if (!elements.has(id)) elements.set(id, new Element());
    return elements.get(id);
  };
  const state = {photos: Array.from({length: 23}, (_, i) => ({id: "id" + i, name: "photo" + i + ".jpg", size: 1024})),
    offsets: [], images: [], aborted: [], deleted: [], confirm: false, confirmations: [], failOffset: null,
    frames: [], endVisibleAt: null, deferred: new Set(), revoked: [], observer: null, savedSettings: []};
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
    FormData: class { constructor(form) { this.values = form.values; } [Symbol.iterator]() { return Object.entries(this.values)[Symbol.iterator](); } },
    URL: {createObjectURL: () => "blob:thumbnail-" + state.images.length, revokeObjectURL: url => state.revoked.push(url)},
    requestAnimationFrame: callback => { state.frames.push(callback); return state.frames.length; },
    setInterval: () => 1, clearInterval() {}, setTimeout: () => 1, clearTimeout() {},
    fetch: async (url, options = {}) => {
      if (url === "/api/device") return response({width: 600, height: 448, sdReady: true, directory: "/landscape"});
      if (url === "/api/settings") {
        if (options.method === "POST") {
          state.savedSettings.push(Object.fromEntries(options.body));
          return response("Settings saved.");
        }
        return response(settings);
      }
      if (url === "/api/ping") return response("");
      if (url.startsWith("/api/photos?")) {
        const offset = Number(url.split("=")[1]);
        state.offsets.push(offset);
        if (offset === state.failOffset) return response("Simulated listing failure", false);
        return response({photos: state.photos.slice(offset, offset + 10), nextOffset: offset + 10 < state.photos.length ? offset + 10 : null,
          totalPhotos: state.photos.length, totalPages: Math.ceil(state.photos.length / 10), pageSize: 10});
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
  const lazy = await harness();
  assert.equal(lazy.byId("nightPauseStart").value, "23:00");
  assert.equal(lazy.byId("nightPauseEnd").value, "05:00");
  assert.equal(lazy.byId("updateMinute").value, "00");
  const settings = await harness({settings: {ssid: "Test WiFi", nightPauseStart: "22:30", nightPauseEnd: "06:15", updateMinute: "45"}});
  assert.equal(settings.byId("nightPauseStart").value, "22:30");
  assert.equal(settings.byId("nightPauseEnd").value, "06:15");
  assert.equal(settings.byId("updateMinute").value, "45");
  const save = async (start, end, minute = "45") => settings.byId("settings-form").listeners.submit({preventDefault() {}, target: {
    values: {ssid: "Test WiFi", password: "", latitude: "48.1", longitude: "11.5", calendarUrl: "", nightPauseStart: start, nightPauseEnd: end, updateMinute: minute}
  }});
  await save("22:30", "06:15");
  assert.equal(settings.state.savedSettings.length, 1);
  assert.equal(settings.state.savedSettings[0].nightPauseStart, "22:30");
  assert.equal(settings.state.savedSettings[0].nightPauseEnd, "06:15");
  assert.equal(settings.state.savedSettings[0].updateMinute, "45");
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
module.exports = {run};
if (require.main === module) run().catch(error => { console.error(error); process.exitCode = 1; });
