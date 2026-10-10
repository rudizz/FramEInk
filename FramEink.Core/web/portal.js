/* All processing is local. Only the final, resized JPEG is sent to FramEInk. */
"use strict";
const byId = id => document.getElementById(id);
const canvas = byId("preview");
const context = canvas.getContext("2d", {willReadFrequently: true});
let device;
let queue = [];
let currentIndex = 0;
let sourceImage = null;
let rotation = 0;
let busy = false;
let closed = false;
let heartbeat = null;
let heicModule = null;
let savedCount = 0;
let lastInteractionPing = 0;
let galleryPhotos = [];
let galleryOffset = 0;
let galleryHistory = [];
let galleryNext = null;
let galleryTotalPhotos = 0;
const gallerySelection = new Map();
let galleryPreviews = [];
let galleryPreviewVersion = 0;
let galleryPreviewController = null;

// Editing the crop or settings is activity too, even before a photo is sent.
function keepAliveOnInteraction() {
  if (!device || closed || Date.now() - lastInteractionPing < 10000) return;
  lastInteractionPing = Date.now();
  fetch("/api/ping", {method: "POST"}).catch(() => {});
}
document.addEventListener("input", keepAliveOnInteraction);
document.addEventListener("click", keepAliveOnInteraction);

function status(id, message, kind = "") {
  const element = byId(id);
  element.textContent = message;
  element.className = "status " + kind;
}

async function request(path, options = {}) {
  const response = await fetch(path, {cache: "no-store", ...options});
  if (!response.ok) throw new Error(await response.text() || "Communication error.");
  return response;
}

function setBusy(value) {
  busy = value;
  if (value || closed) pauseGalleryPreviews();
  byId("finish").disabled = value || !device || closed;
  byId("files").disabled = value || !device || !device.sdReady || closed;
  byId("pick-label").classList.toggle("disabled", byId("files").disabled);
  for (const id of ["upload", "skip", "rotate", "fit", "crop-x", "crop-y", "save-settings"]) {
    byId(id).disabled = value || closed;
  }
  byId("upload").disabled ||= !sourceImage;
  byId("rotate").disabled ||= !sourceImage;
  updateGalleryControls();
  if (heartbeat) clearInterval(heartbeat);
  heartbeat = null;
  if (value) {
    // Keep the AP alive while the phone decodes HEIC, before any upload starts.
    const ping = () => fetch("/api/ping", {method: "POST"}).catch(() => {});
    ping();
    heartbeat = setInterval(ping, 10000);
  }
  if (!value && !closed) resumeGalleryPreviews();
}

function loadImage(blob) {
  return new Promise((resolve, reject) => {
    const url = URL.createObjectURL(blob);
    const image = new Image();
    image.onload = () => { URL.revokeObjectURL(url); resolve(image); };
    image.onerror = () => { URL.revokeObjectURL(url); reject(new Error("Unable to read this image format.")); };
    image.src = url;
  });
}

async function isHeic(file) {
  if (/\.hei[cf]$/i.test(file.name) || /^image\/hei[cf]/i.test(file.type)) return true;
  const header = new Uint8Array(await file.slice(0, 64).arrayBuffer());
  const brands = new TextDecoder().decode(header);
  return /ftyp(?:heic|heix|hevc|hevx|mif1|msf1)/.test(brands);
}

async function decodePhoto(file) {
  try {
    // Safari or the iOS photo picker may already decode/convert HEIC natively.
    return await loadImage(file);
  } catch (nativeError) {
    if (!await isHeic(file)) throw nativeError;
    status("photo-status", "Converting HEIC on your phone… The first conversion may take a few seconds.");
    if (!heicModule) heicModule = import("/heic-to.js").catch(error => { heicModule = null; throw error; });
    const decoder = await heicModule;
    let timeout;
    let jpeg;
    try {
      jpeg = await Promise.race([
        decoder.heicTo({blob: file, type: "image/jpeg", quality: 0.95}),
        new Promise((_, reject) => {
          timeout = setTimeout(() => reject(new Error("HEIC conversion took more than two minutes. Try a smaller photo.")), 120000);
        })
      ]);
    } finally { clearTimeout(timeout); }
    return loadImage(jpeg);
  }
}

function drawPreview() {
  if (!sourceImage) return;
  canvas.width = device.width;
  canvas.height = device.height;
  context.fillStyle = "#ffffff";
  context.fillRect(0, 0, canvas.width, canvas.height);
  const quarterTurn = rotation % 180 !== 0;
  const sourceWidth = quarterTurn ? sourceImage.naturalHeight : sourceImage.naturalWidth;
  const sourceHeight = quarterTurn ? sourceImage.naturalWidth : sourceImage.naturalHeight;
  const cover = byId("fit").value === "cover";
  const scale = (cover ? Math.max : Math.min)(canvas.width / sourceWidth, canvas.height / sourceHeight);
  const width = sourceWidth * scale;
  const height = sourceHeight * scale;
  const x = (canvas.width - width) * (cover ? Number(byId("crop-x").value) / 100 : 0.5);
  const y = (canvas.height - height) * (cover ? Number(byId("crop-y").value) / 100 : 0.5);
  context.save();
  context.translate(x + width / 2, y + height / 2);
  context.rotate(rotation * Math.PI / 180);
  context.imageSmoothingEnabled = true;
  context.imageSmoothingQuality = "high";
  context.drawImage(sourceImage,
    -sourceImage.naturalWidth * scale / 2, -sourceImage.naturalHeight * scale / 2,
    sourceImage.naturalWidth * scale, sourceImage.naturalHeight * scale);
  context.restore();
  if (device.grayscale) {
    const pixels = context.getImageData(0, 0, canvas.width, canvas.height);
    for (let i = 0; i < pixels.data.length; i += 4) {
      const gray = Math.round(0.299 * pixels.data[i] + 0.587 * pixels.data[i + 1] + 0.114 * pixels.data[i + 2]);
      pixels.data[i] = pixels.data[i + 1] = pixels.data[i + 2] = gray;
    }
    context.putImageData(pixels, 0, 0);
  }
  byId("crop-controls").hidden = !cover;
}

async function showCurrent() {
  sourceImage = null;
  canvas.width = canvas.height = 1;
  byId("editor").hidden = true;
  byId("progress").hidden = true;
  if (currentIndex >= queue.length) {
    byId("queue-info").textContent = "Selection complete. You can add more photos or tap Finish.";
    status("photo-status", savedCount ? savedCount + " photo(s) saved to the SD card." : "No photos uploaded.", savedCount ? "success" : "");
    return;
  }
  setBusy(true);
  const file = queue[currentIndex];
  byId("queue-info").textContent = "Photo " + (currentIndex + 1) + " of " + queue.length + " · " + file.name;
  status("photo-status", "Preparing the preview…");
  try {
    if (file.size > 40 * 1024 * 1024) throw new Error("The photo exceeds 40 MB. Choose a smaller version.");
    sourceImage = await decodePhoto(file);
    rotation = 0;
    byId("crop-x").value = byId("crop-y").value = "50";
    drawPreview();
    byId("editor").hidden = false;
    status("photo-status", "Ready: " + device.width + " × " + device.height + " pixels · JPEG. Check the preview and tap Upload photo.");
  } catch (error) {
    byId("editor").hidden = false;
    status("photo-status", "Unable to prepare this photo: " + (error.message || String(error)) + "\nYou can skip it or select another image.", "error");
  } finally {
    setBusy(false);
    byId("upload").disabled = !sourceImage;
    byId("rotate").disabled = !sourceImage;
  }
}

function jpegBlob() {
  return new Promise((resolve, reject) => {
    canvas.toBlob(blob => {
      if (blob && blob.type === "image/jpeg") resolve(blob);
      else reject(new Error("The browser could not create the JPEG."));
    }, "image/jpeg", 0.92);
  });
}

function transferPhoto(blob, name) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open("POST", "/api/photos");
    xhr.timeout = 120000;
    xhr.upload.onprogress = event => {
      if (event.lengthComputable) byId("progress").value = 100 * event.loaded / event.total;
    };
    xhr.onerror = () => reject(new Error("Connection interrupted. Check that you are still connected to FramEInk WiFi."));
    xhr.ontimeout = () => reject(new Error("Transfer timed out. Please try again."));
    xhr.onload = () => {
      if (xhr.status !== 201) reject(new Error(xhr.responseText || "Unable to save the photo."));
      else {
        try { resolve(JSON.parse(xhr.responseText)); }
        catch (_) { reject(new Error("Unexpected device response. Check whether the photo was saved before trying again.")); }
      }
    };
    const form = new FormData();
    form.append("photo", blob, name.replace(/\.[^.]*$/, "") + ".jpg");
    xhr.send(form);
  });
}

byId("files").addEventListener("change", async event => {
  if (busy || closed) return;
  queue = Array.from(event.target.files || []);
  currentIndex = 0;
  event.target.value = "";
  await showCurrent();
});
byId("fit").addEventListener("change", drawPreview);
byId("crop-x").addEventListener("input", drawPreview);
byId("crop-y").addEventListener("input", drawPreview);
byId("rotate").addEventListener("click", () => { rotation = (rotation + 90) % 360; drawPreview(); });
byId("skip").addEventListener("click", async () => { currentIndex++; await showCurrent(); });
byId("upload").addEventListener("click", async () => {
  if (busy || !sourceImage || closed) return;
  setBusy(true);
  byId("progress").hidden = false;
  byId("progress").value = 0;
  status("photo-status", "Uploading and saving to the SD card…");
  try {
    const blob = await jpegBlob();
    if (blob.size > device.maxPhotoBytes) throw new Error("JPEG too large for this device.");
    const result = await transferPhoto(blob, queue[currentIndex].name);
    const item = document.createElement("li");
    item.textContent = queue[currentIndex].name + " → " + result.path;
    byId("uploaded").appendChild(item);
    savedCount++;
    currentIndex++;
    await loadGallery(0, []);
    await showCurrent();
  } catch (error) {
    status("photo-status", error.message || String(error), "error");
    byId("progress").hidden = true;
    setBusy(false);
  }
});
byId("settings-form").addEventListener("submit", async event => {
  event.preventDefault();
  if (busy || closed) return;
  setBusy(true);
  try {
    const body = new URLSearchParams(new FormData(event.target));
    const response = await request("/api/settings", {method: "POST", body});
    status("settings-status", await response.text(), "success");
  } catch (error) { status("settings-status", error.message, "error"); }
  finally { setBusy(false); }
});
byId("finish").addEventListener("click", async () => {
  if (busy || closed) return;
  setBusy(true);
  try {
    const response = await request("/api/finish", {method: "POST"});
    const message = await response.text();
    closed = true;
    byId("finish").textContent = "Finished";
    status("photo-status", message, "success");
  } catch (error) {
    byId("settings").open = true;
    status("settings-status", error.message, "error");
  } finally { setBusy(false); }
});

function selectedPhotos() {
  return Array.from(gallerySelection.values());
}

function syncGallerySelection() {
  for (const checkbox of byId("gallery").querySelectorAll("input")) {
    checkbox.checked = gallerySelection.has(checkbox.value);
  }
  updateGalleryControls();
}

function updateGalleryControls() {
  const unavailable = busy || closed || !device || !device.sdReady;
  const selected = selectedPhotos().length;
  const selectedOnPage = galleryPhotos.filter(photo => gallerySelection.has(photo.id)).length;
  byId("refresh-photos").disabled = unavailable;
  byId("select-photos").disabled = unavailable || !galleryPhotos.length;
  byId("select-photos").textContent = selectedOnPage && selectedOnPage === galleryPhotos.length ? "Clear this page" : "Select this page";
  byId("select-all-photos").disabled = unavailable || !galleryTotalPhotos;
  byId("clear-photos").disabled = unavailable || !selected;
  byId("clear-photos").hidden = !selected;
  byId("gallery-selection").hidden = !selected;
  byId("gallery-selection").textContent = selected ? selected + " photo(s) selected across all pages." : "";
  byId("delete-photos").disabled = unavailable || !selected;
  byId("delete-photos").textContent = selected ? "Delete selected (" + selected + ")" : "Delete selected";
  byId("previous-photos").disabled = unavailable || !galleryHistory.length;
  byId("next-photos").disabled = unavailable || galleryNext === null;
  for (const control of byId("gallery").querySelectorAll("input,button")) control.disabled = unavailable;
}

function pauseGalleryPreviews() {
  galleryPreviewVersion++;
  if (galleryPreviewController) galleryPreviewController.abort();
  galleryPreviewController = null;
}

async function resumeGalleryPreviews() {
  if (busy || closed || !device || !device.sdReady || galleryPreviewController) return;
  // Serialize thumbnail downloads. Stop immediately for uploads, page changes,
  // deletion or opening a large photo; incomplete previews resume afterwards.
  const version = ++galleryPreviewVersion;
  for (const job of galleryPreviews) {
    if (job.loaded) continue;
    if (version !== galleryPreviewVersion || busy || closed) return;
    const controller = new AbortController();
    galleryPreviewController = controller;
    const timeout = setTimeout(() => controller.abort(), 45000);
    try {
      const response = await fetch("/api/photo?id=" + encodeURIComponent(job.photo.id), {
        cache: "no-store", signal: controller.signal
      });
      if (!response.ok) throw new Error("Photo unavailable.");
      const blob = await response.blob();
      if (version !== galleryPreviewVersion) return;
      const image = await loadImage(blob);
      if (version !== galleryPreviewVersion || busy || closed) return;
      // Keep only a small canvas per photo, rather than ten full-size decoded
      // images. Resizing runs on the phone and does not change files on the SD.
      const ctx = job.canvas.getContext("2d");
      const scale = Math.min(job.canvas.width / image.naturalWidth, job.canvas.height / image.naturalHeight);
      const width = image.naturalWidth * scale, height = image.naturalHeight * scale;
      ctx.fillStyle = "#ffffff";
      ctx.fillRect(0, 0, job.canvas.width, job.canvas.height);
      ctx.imageSmoothingEnabled = true;
      ctx.imageSmoothingQuality = "high";
      ctx.drawImage(image, (job.canvas.width - width) / 2, (job.canvas.height - height) / 2, width, height);
      job.note.hidden = true;
      job.loaded = true;
    } catch (error) {
      if (version !== galleryPreviewVersion) return;
      job.note.textContent = "Preview unavailable. You can still select this file or try View photo.";
      job.loaded = true;
    } finally {
      clearTimeout(timeout);
      if (galleryPreviewController === controller) galleryPreviewController = null;
    }
  }
}

async function loadGallery(offset = galleryOffset, history = galleryHistory) {
  if (!device || !device.sdReady || closed) return;
  setBusy(true);
  status("gallery-status", "Loading photos from the SD card…");
  try {
    const result = await (await request("/api/photos?offset=" + offset)).json();
    galleryPhotos = result.photos;
    galleryOffset = offset;
    galleryHistory = [...history];
    galleryNext = result.nextOffset;
    galleryTotalPhotos = result.totalPhotos;
    byId("gallery").replaceChildren();
    galleryPreviews = [];
    for (const photo of galleryPhotos) {
      const card = document.createElement("article");
      card.className = "photo-card";
      const label = document.createElement("label");
      const checkbox = document.createElement("input");
      checkbox.type = "checkbox";
      checkbox.value = photo.id;
      checkbox.checked = gallerySelection.has(photo.id);
      checkbox.addEventListener("change", () => {
        if (checkbox.checked) gallerySelection.set(photo.id, photo);
        else gallerySelection.delete(photo.id);
        updateGalleryControls();
      });
      const name = document.createElement("span");
      name.textContent = photo.name;
      label.append(checkbox, name);
      const thumbnail = document.createElement("canvas");
      thumbnail.className = "photo-thumb";
      thumbnail.width = 320;
      thumbnail.height = 240;
      thumbnail.setAttribute("role", "img");
      thumbnail.setAttribute("aria-label", "Preview of " + photo.name);
      const note = document.createElement("p");
      note.className = "preview-note";
      note.textContent = "Loading preview…";
      galleryPreviews.push({photo, canvas: thumbnail, note, loaded: false});
      const size = document.createElement("small");
      size.textContent = Math.max(1, Math.round(photo.size / 1024)) + " KB";
      const view = document.createElement("button");
      view.className = "secondary";
      view.textContent = "View photo";
      view.setAttribute("aria-label", "View " + photo.name);
      view.addEventListener("click", () => viewPhoto(photo));
      card.append(label, thumbnail, note, size, view);
      byId("gallery").appendChild(card);
    }
    const page = Math.floor(galleryOffset / result.pageSize) + 1;
    byId("gallery-page").hidden = !galleryPhotos.length;
    byId("gallery-page").textContent = galleryPhotos.length ? "Page " + page + "\\" + result.totalPages + " · " + galleryPhotos.length + " photo(s)." : "";
    status("gallery-status", galleryPhotos.length ? "" : "No photos in this folder.");
  } catch (error) {
    status("gallery-status", "Unable to load photos: " + error.message + " Tap Refresh to try again.", "error");
  } finally { setBusy(false); }
}

function viewPhoto(photo) {
  if (busy || closed) return;
  const image = byId("stored-photo");
  byId("view-title").textContent = photo.name;
  image.alt = photo.name;
  image.hidden = true;
  status("view-status", "Loading photo…");
  byId("photo-dialog").showModal();
  setBusy(true);
  image.onload = () => {
    if (!byId("photo-dialog").open) return;
    image.hidden = false;
    status("view-status", "");
    setBusy(false);
  };
  image.onerror = () => {
    if (!byId("photo-dialog").open) return;
    status("view-status", "Unable to display this photo. Close the preview and refresh the gallery.", "error");
    setBusy(false);
  };
  image.src = "/api/photo?id=" + encodeURIComponent(photo.id);
}

byId("photo-dialog").addEventListener("close", () => {
  byId("stored-photo").removeAttribute("src");
  setBusy(false);
});
byId("refresh-photos").addEventListener("click", () => {
  gallerySelection.clear();
  syncGallerySelection();
  return loadGallery(0, []);
});
byId("previous-photos").addEventListener("click", () => loadGallery(galleryHistory[galleryHistory.length - 1], galleryHistory.slice(0, -1)));
byId("next-photos").addEventListener("click", () => loadGallery(galleryNext, [...galleryHistory, galleryOffset]));
byId("select-photos").addEventListener("click", () => {
  if (busy || closed) return;
  const select = !galleryPhotos.every(photo => gallerySelection.has(photo.id));
  for (const photo of galleryPhotos) {
    if (select) gallerySelection.set(photo.id, photo);
    else gallerySelection.delete(photo.id);
  }
  syncGallerySelection();
});
byId("clear-photos").addEventListener("click", () => {
  if (busy || closed) return;
  gallerySelection.clear();
  syncGallerySelection();
});
byId("select-all-photos").addEventListener("click", async () => {
  if (busy || closed || !device || !device.sdReady) return;
  setBusy(true);
  const selection = new Map();
  let offset = 0;
  try {
    // Collect a snapshot of explicit IDs, page by page. No unbounded list is
    // allocated on the ESP32, and photos uploaded later are not auto-selected.
    while (true) {
      const result = await (await request("/api/photos?offset=" + offset)).json();
      for (const photo of result.photos) selection.set(photo.id, photo);
      status("gallery-status", "Selecting all photos… " + selection.size + " of " + result.totalPhotos + ".");
      if (result.nextOffset === null) break;
      if (result.nextOffset <= offset) throw new Error("Invalid gallery page.");
      offset = result.nextOffset;
    }
    gallerySelection.clear();
    for (const [id, photo] of selection) gallerySelection.set(id, photo);
    syncGallerySelection();
    // The live selection summary owns the count; don't leave a stale copy here
    // after the user deselects photos or clears the selection.
    status("gallery-status", "");
  } catch (error) {
    // An incomplete listing must never silently replace the user's selection.
    status("gallery-status", "Unable to select all photos: " + error.message + "\nYour previous selection has been kept.", "error");
  } finally { setBusy(false); }
});
byId("delete-photos").addEventListener("click", async () => {
  if (busy || closed) return;
  const selected = selectedPhotos();
  const names = selected.slice(0, 20).map(photo => photo.name).join("\n");
  const remaining = selected.length > 20 ? "\n… and " + (selected.length - 20) + " more." : "";
  if (!selected.length || !window.confirm("Permanently delete " + selected.length + " selected photo(s) from the SD card, including selections on other pages? This cannot be undone.\n\n" + names + remaining)) return;
  setBusy(true);
  let deleted = 0;
  let failure = "";
  try {
    for (const photo of selected) {
      status("gallery-status", "Deleting photo " + (deleted + 1) + " of " + selected.length + "…");
      await request("/api/photos/delete", {method: "POST", headers: {"X-FramEInk-Request": "1"}, body: new URLSearchParams({id: photo.id})});
      gallerySelection.delete(photo.id);
      deleted++;
    }
  } catch (error) { failure = error.message; }
  // Refresh even after a partial failure; never claim that every file was deleted.
  await loadGallery(0, []);
  status("gallery-status", deleted + " photo(s) deleted." + (failure ? "\nDeletion stopped: " + failure : ""), failure ? "error" : "success");
  setBusy(false);
});

(async function init() {
  try {
    // The ESP32 WebServer accepts one client at a time.
    device = await (await request("/api/device")).json();
    if (!device.width || !device.height) throw new Error("Device resolution unavailable.");
    const settings = await (await request("/api/settings")).json();
    for (const key of ["ssid", "password", "latitude", "longitude", "calendarUrl"]) byId(key).value = settings[key] || "";
    byId("settings").open = !settings.ssid;
    byId("device").textContent = device.name + " · " + device.width + " × " + device.height + " px" + (device.grayscale ? " · grayscale" : " · color");
    byId("gallery-folder").textContent = "SD card · " + device.directory + ". View photos or select them for deletion.";
    if (!device.sdReady) {
      status("photo-status", "SD card unavailable. Insert it and restart the frame.", "error");
      status("gallery-status", "SD card unavailable. Insert it and restart the frame.", "error");
    }
    setBusy(false);
    await loadGallery(0, []);
  } catch (error) {
    device = null;
    status("photo-status", "Unable to connect to FramEInk: " + error.message + " Reload the page while the portal is open.", "error");
  }
})();
