"use strict";
// Test the real portal script against an in-memory DOM/server. No browser,
// network, real SD files or user photos are touched, including deletion tests.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

class Element {
  constructor(tag = "div") {
    this.tag = tag;
    this.children = [];
    this.listeners = {};
    this.textContent = "";
    this.classList = {toggle() {}};
  }
  addEventListener(name, handler) { this.listeners[name] = handler; }
  setAttribute() {}
  getContext() { return {}; }
  append(...children) { this.children.push(...children); }
  appendChild(child) { this.append(child); }
  replaceChildren() { this.children = []; }
  querySelectorAll(selector) {
    const descendants = this.children.flatMap(child => [child, ...child.querySelectorAll("*")]);
    if (selector === "*") return descendants;
    if (selector === "input:checked") return descendants.filter(child => child.tag === "input" && child.checked);
    return descendants.filter(child => selector.split(",").includes(child.tag));
  }
}

async function harness() {
  const elements = new Map();
  const byId = id => {
    if (!elements.has(id)) elements.set(id, new Element());
    return elements.get(id);
  };
  const state = {photos: Array.from({length: 23}, (_, index) => ({
    id: "id" + index, name: "photo" + index + ".jpg", size: 1024
  })), offsets: [], deleted: [], confirm: false, confirmations: [], failOffset: null};
  const response = (body, ok = true) => ({ok, json: async () => body, text: async () => String(body)});
  const context = vm.createContext({
    document: {getElementById: byId, createElement: tag => new Element(tag), addEventListener() {}},
    window: {confirm: text => { state.confirmations.push(text); return state.confirm; }},
    URLSearchParams, setInterval: () => 1, clearInterval() {}, setTimeout: () => 1, clearTimeout() {},
    fetch: async (url, options = {}) => {
      if (url === "/api/device") return response({width: 600, height: 448, sdReady: true, directory: "/landscape"});
      if (url === "/api/settings") return response({ssid: "Test WiFi"});
      if (url === "/api/ping") return response("");
      if (url.startsWith("/api/photos?")) {
        const offset = Number(url.split("=")[1]);
        state.offsets.push(offset);
        if (offset === state.failOffset) return response("Simulated listing failure", false);
        return response({photos: state.photos.slice(offset, offset + 10),
          nextOffset: offset + 10 < state.photos.length ? offset + 10 : null,
          totalPhotos: state.photos.length, totalPages: Math.ceil(state.photos.length / 10), pageSize: 10});
      }
      if (url === "/api/photos/delete") {
        const id = options.body.get("id");
        assert.equal(options.headers["X-FramEInk-Request"], "1");
        state.deleted.push(id);
        state.photos = state.photos.filter(photo => photo.id !== id);
        return response("Photo deleted.");
      }
      throw new Error("Unexpected test request: " + url);
    }
  });
  const source = fs.readFileSync(path.join(__dirname, "../FramEink.Core/web/portal.js"), "utf8");
  vm.runInContext(source, context);
  // Image conversion/thumbnail decoding is covered by browser smoke tests.
  vm.runInContext("resumeGalleryPreviews = () => {};", context);
  await new Promise(resolve => setImmediate(resolve));
  const click = id => byId(id).listeners.click();
  const boxes = () => byId("gallery").querySelectorAll("input");
  const count = () => vm.runInContext("gallerySelection.size", context);
  const check = (index, checked) => {
    const box = boxes()[index];
    box.checked = checked;
    box.listeners.change();
  };
  return {state, byId, click, boxes, count, check};
}

async function run() {
  const h = await harness();
  assert.equal(h.boxes().length, 10);
  h.state.offsets = [];
  await h.click("select-all-photos");
  assert.deepEqual(h.state.offsets, [0, 10, 20]);
  assert.equal(h.count(), 23);
  assert(h.boxes().every(box => box.checked));
  assert.equal(h.byId("delete-photos").textContent, "Delete selected (23)");
  assert.equal(h.byId("gallery-selection").textContent, "23 photo(s) selected across all pages.");
  assert.equal(h.byId("gallery-status").textContent, "");
  assert.match(h.byId("gallery-page").textContent, /Page 1\\3/);
  await h.click("next-photos");
  assert.equal(h.count(), 23);
  assert(h.boxes().every(box => box.checked));
  h.check(0, false);
  assert.equal(h.count(), 22);
  await h.click("select-photos"); // Fill the partial page.
  assert.equal(h.count(), 23);
  await h.click("select-photos"); // Clear only this page.
  assert.equal(h.count(), 13);
  assert(h.boxes().every(box => !box.checked));
  await h.click("clear-photos");
  assert.equal(h.count(), 0);
  assert.equal(h.byId("gallery-selection").hidden, true);
  assert.equal(h.byId("gallery-status").textContent, "");
  h.check(0, true);
  await h.click("next-photos");
  h.check(0, true);
  assert.equal(h.count(), 2);
  await h.click("delete-photos"); // Cancellation preserves selections and files.
  assert.equal(h.state.deleted.length, 0);
  assert.equal(h.count(), 2);
  h.state.confirm = true;
  await h.click("delete-photos");
  assert.deepEqual(h.state.deleted, ["id10", "id20"]);
  assert.equal(h.count(), 0);
  assert.equal(h.state.photos.length, 21);
  await h.click("select-all-photos");
  assert.equal(h.count(), 21);
  await h.click("delete-photos"); // Only simulated in-memory fixtures.
  assert.equal(h.state.photos.length, 0);
  assert.equal(h.count(), 0);
  assert.equal(h.byId("select-all-photos").disabled, true);
  assert.equal(h.byId("gallery-page").hidden, true);
  assert.match(h.state.confirmations.at(-1), /21 selected photo/);
  assert.match(h.state.confirmations.at(-1), /and 1 more/);

  const failure = await harness();
  failure.check(0, true);
  failure.state.failOffset = 10;
  await failure.click("select-all-photos");
  assert.equal(failure.count(), 1);
  assert.equal(failure.boxes()[0].checked, true);
  assert.match(failure.byId("gallery-status").textContent, /previous selection has been kept/);
  failure.state.failOffset = null;
  await failure.click("refresh-photos");
  assert.equal(failure.count(), 0);
  console.log("Portal selection: all-page selection, navigation, deselection, confirmation, deletion and listing failure passed.");
}

module.exports = {run};
if (require.main === module) run().catch(error => { console.error(error); process.exitCode = 1; });
