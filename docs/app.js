(function () {
  const versionEl = document.getElementById("firmware-version");
  const versionSelectEl = document.getElementById("firmware-version-select");
  const browserEl = document.getElementById("browser-check");
  const secureEl = document.getElementById("secure-check");
  const latestChangelogEl = document.getElementById("latest-changelog");
  const changelogLinkEl = document.getElementById("changelog-link");
  const installBtnEl = document.getElementById("install-btn");
  const debugPanelEl = document.getElementById("debug-panel");
  const debugLogEl = document.getElementById("debug-log");
  const debugCopyBtnEl = document.getElementById("debug-copy");
  const FIRMWARE_FETCH_TIMEOUT_MS = 30000;
  const DEBUG_QUERY_KEY = "debug";
  const DEBUG_LOG_MAX_ENTRIES = 400;
  const SERIAL_FILTER_DISABLE_QUERY_KEY = "allports";
  const ALLOW_RUNTIME_PORT_QUERY_KEY = "allowruntime";
  const ADAFRUIT_USB_VENDOR_ID = 0x239a;
  const ADAFRUIT_RUNTIME_PID_MASK = 0x8000;
  const ADAFRUIT_TOUCH_1200_BAUD = 1200;
  const ADAFRUIT_BOOTLOADER_POLL_TIMEOUT_MS = 4200;
  const ADAFRUIT_BOOTLOADER_POLL_INTERVAL_MS = 140;
  const VERSION_INDEX_PATH = "./firmware/versions.json";
  const FALLBACK_MANIFEST_PATH = "./firmware/latest/manifest.json";
  const DEFAULT_SERIAL_FILTERS = [
    // ESP32-S3 ROM download mode over native USB.
    { usbVendorId: 0x303a, usbProductId: 0x1001 },
    { usbVendorId: 0x303a },
    // Boards that expose UF2/CDC or USB-UART bridges.
    { usbVendorId: 0x239a },
    { usbVendorId: 0x10c4 },
    { usbVendorId: 0x1a86 },
    { usbVendorId: 0x0403 },
  ];
  const queryParams = new URLSearchParams(window.location.search);

  function parseBooleanQueryValue(value) {
    const normalized = (value || "").trim().toLowerCase();
    if (["1", "true", "yes", "on"].includes(normalized)) return true;
    if (["0", "false", "no", "off"].includes(normalized)) return false;
    return null;
  }

  function isQueryFlagEnabled(queryKey) {
    return parseBooleanQueryValue(queryParams.get(queryKey)) === true;
  }

  function shouldDisableSerialFiltering() {
    return isQueryFlagEnabled(SERIAL_FILTER_DISABLE_QUERY_KEY);
  }

  function shouldAllowRuntimePortFlashing() {
    return isQueryFlagEnabled(ALLOW_RUNTIME_PORT_QUERY_KEY);
  }

  function delay(ms) {
    return new Promise((resolve) => {
      setTimeout(resolve, ms);
    });
  }

  function toHex16(value) {
    return `0x${(Number(value) >>> 0).toString(16).toUpperCase()}`;
  }

  function isAdafruitRuntimePort(usbVendorId, usbProductId) {
    return (
      usbVendorId === ADAFRUIT_USB_VENDOR_ID &&
      (usbProductId & ADAFRUIT_RUNTIME_PID_MASK) === ADAFRUIT_RUNTIME_PID_MASK
    );
  }

  function runtimePidToBootloaderPid(usbProductId) {
    return usbProductId & ~ADAFRUIT_RUNTIME_PID_MASK;
  }

  function matchesAdafruitBootloaderPort(serialInfo, expectedBootloaderPid) {
    const usbVendorId = Number(serialInfo.usbVendorId || 0);
    const usbProductId = Number(serialInfo.usbProductId || 0);
    if (usbVendorId !== ADAFRUIT_USB_VENDOR_ID) return false;
    if (!expectedBootloaderPid) return true;
    return usbProductId === expectedBootloaderPid;
  }

  async function trySwitchAdafruitRuntimeToBootloader(runtimePort, runtimePid) {
    const expectedBootloaderPid = runtimePidToBootloaderPid(runtimePid);
    let openedByGuard = false;

    try {
      if (!runtimePort.readable || !runtimePort.writable) {
        await runtimePort.open({ baudRate: ADAFRUIT_TOUCH_1200_BAUD, bufferSize: 256 });
        openedByGuard = true;
        debug.log("serial-runtime-touch-opened", {
          baudRate: ADAFRUIT_TOUCH_1200_BAUD,
          expectedBootloaderPid,
        });
      }
      if (typeof runtimePort.setSignals === "function") {
        try {
          await runtimePort.setSignals({ dataTerminalReady: true, requestToSend: false });
          await delay(30);
          await runtimePort.setSignals({ dataTerminalReady: false, requestToSend: false });
          debug.log("serial-runtime-touch-signals-sent", { expectedBootloaderPid });
        } catch (signalError) {
          debug.log("serial-runtime-touch-signals-failed", { error: signalError });
        }
      }
    } catch (openError) {
      debug.log("serial-runtime-touch-failed", { error: openError });
    } finally {
      if (openedByGuard && (runtimePort.readable || runtimePort.writable)) {
        try {
          await runtimePort.close();
          debug.log("serial-runtime-touch-closed");
        } catch (closeError) {
          debug.log("serial-runtime-touch-close-failed", { error: closeError });
        }
      }
    }

    const deadline = Date.now() + ADAFRUIT_BOOTLOADER_POLL_TIMEOUT_MS;
    while (Date.now() < deadline) {
      try {
        const ports = await navigator.serial.getPorts();
        const bootloaderPort = ports.find((portCandidate) => {
          if (typeof portCandidate.getInfo !== "function") return false;
          const candidateInfo = portCandidate.getInfo();
          return matchesAdafruitBootloaderPort(candidateInfo, expectedBootloaderPid);
        });
        if (bootloaderPort) {
          const info = bootloaderPort.getInfo();
          debug.log("serial-runtime-switched-port-found", {
            usbVendorId: Number(info.usbVendorId || 0),
            usbProductId: Number(info.usbProductId || 0),
            expectedBootloaderPid,
          });
          return bootloaderPort;
        }
      } catch (error) {
        debug.log("serial-runtime-switch-poll-failed", { error });
      }

      await delay(ADAFRUIT_BOOTLOADER_POLL_INTERVAL_MS);
    }

    return null;
  }

  function createDebugLogger() {
    const debugValue = (queryParams.get(DEBUG_QUERY_KEY) || "").toLowerCase();
    const enabled = debugValue === "1" || debugValue === "true";
    const entries = [];
    let nextId = 1;

    const serializeDetails = (details) => {
      if (typeof details === "undefined") return undefined;
      const seen = new WeakSet();

      try {
        const json = JSON.stringify(details, (key, value) => {
          if (value instanceof Error) {
            return {
              name: value.name,
              message: value.message,
              stack: typeof value.stack === "string" ? value.stack.split("\n").slice(0, 4) : [],
            };
          }
          if (value instanceof Event) {
            return { type: value.type, isTrusted: value.isTrusted };
          }
          if (typeof value === "bigint") {
            return value.toString();
          }
          if (typeof value === "function") {
            return "[function]";
          }
          if (typeof value === "object" && value !== null) {
            if (seen.has(value)) {
              return "[circular]";
            }
            seen.add(value);
          }
          return value;
        });
        if (typeof json !== "string") return String(details);
        return JSON.parse(json);
      } catch (error) {
        return String(details);
      }
    };

    const render = () => {
      if (!enabled || !debugLogEl) return;
      const lines = entries.map((entry) => {
        const details =
          typeof entry.details === "undefined" ? "" : ` ${JSON.stringify(entry.details)}`;
        const clippedDetails =
          details.length > 280 ? `${details.slice(0, 277)}...` : details;
        return `[${entry.timestamp}] ${entry.event}${clippedDetails}`;
      });
      debugLogEl.textContent = lines.join("\n");
      debugLogEl.scrollTop = debugLogEl.scrollHeight;
    };

    const addEntry = (event, details) => {
      if (!enabled) return;
      entries.push({
        id: nextId,
        timestamp: new Date().toISOString(),
        event,
        details: serializeDetails(details),
      });
      nextId += 1;
      if (entries.length > DEBUG_LOG_MAX_ENTRIES) {
        entries.splice(0, entries.length - DEBUG_LOG_MAX_ENTRIES);
      }
      render();
    };

    const buildReport = () => ({
      generatedAt: new Date().toISOString(),
      page: window.location.href,
      userAgent: navigator.userAgent,
      platform: navigator.platform || "",
      language: navigator.language || "",
      secureContext: window.isSecureContext,
      entries,
    });

    const copyReport = async () => {
      const report = JSON.stringify(buildReport(), null, 2);
      if (navigator.clipboard && typeof navigator.clipboard.writeText === "function") {
        await navigator.clipboard.writeText(report);
        addEntry("debug-report-copied", { method: "clipboard" });
        return;
      }

      const filename = `mrf2-web-updater-debug-${Date.now()}.json`;
      const blob = new Blob([report], { type: "application/json" });
      const downloadUrl = URL.createObjectURL(blob);
      const link = document.createElement("a");
      link.href = downloadUrl;
      link.download = filename;
      document.body.appendChild(link);
      link.click();
      link.remove();
      URL.revokeObjectURL(downloadUrl);
      addEntry("debug-report-downloaded", { filename });
    };

    if (enabled) {
      if (debugPanelEl) {
        debugPanelEl.hidden = false;
      }
      if (debugCopyBtnEl) {
        debugCopyBtnEl.addEventListener("click", () => {
          copyReport().catch((error) => {
            addEntry("debug-report-copy-failed", { error });
          });
        });
      }

      window.addEventListener("error", (event) => {
        addEntry("window-error", {
          message: event.message,
          source: event.filename,
          line: event.lineno,
          column: event.colno,
          error: event.error || null,
        });
      });

      window.addEventListener("unhandledrejection", (event) => {
        addEntry("window-unhandledrejection", { reason: event.reason });
      });

      document.addEventListener("visibilitychange", () => {
        addEntry("page-visibility", { visibilityState: document.visibilityState });
      });

      addEntry("debug-enabled", {
        query: window.location.search,
        hint: "Share this report with support when installation hangs.",
      });
    }

    return {
      enabled,
      log: addEntry,
    };
  }

  const debug = createDebugLogger();
  function installFirmwareFetchTimeoutGuard(debugLogger) {
    if (typeof window.fetch !== "function") return;
    const originalFetch = window.fetch.bind(window);

    const isFirmwareArtifactRequest = (input) => {
      try {
        const rawUrl = typeof input === "string" ? input : input && input.url;
        if (!rawUrl) return false;
        const url = new URL(rawUrl, window.location.href);
        if (!url.pathname.includes("/firmware/")) return false;
        return /\.(bin|json)$/i.test(url.pathname);
      } catch (error) {
        return false;
      }
    };

    const resolveRequestUrl = (input) => {
      const rawUrl = typeof input === "string" ? input : input && input.url;
      if (!rawUrl) return "";
      return new URL(rawUrl, window.location.href).href;
    };

    window.fetch = async (input, init) => {
      if (!isFirmwareArtifactRequest(input)) {
        return originalFetch(input, init);
      }

      const requestUrl = resolveRequestUrl(input);
      const startedAt = performance.now();
      const controller = new AbortController();
      const upstreamSignal = init && init.signal;
      let timedOut = false;
      let upstreamAbortListener = null;

      debugLogger.log("firmware-fetch-start", {
        url: requestUrl,
        timeoutMs: FIRMWARE_FETCH_TIMEOUT_MS,
      });

      if (upstreamSignal && typeof upstreamSignal.addEventListener === "function") {
        if (upstreamSignal.aborted) {
          debugLogger.log("firmware-fetch-aborted-upstream", { url: requestUrl });
          controller.abort();
        } else {
          upstreamAbortListener = () => controller.abort();
          upstreamSignal.addEventListener("abort", upstreamAbortListener, { once: true });
        }
      }

      const timeoutId = setTimeout(() => {
        timedOut = true;
        controller.abort();
      }, FIRMWARE_FETCH_TIMEOUT_MS);

      try {
        const response = await originalFetch(input, {
          ...(init || {}),
          signal: controller.signal,
        });
        debugLogger.log("firmware-fetch-end", {
          url: requestUrl,
          status: response.status,
          ok: response.ok,
          durationMs: Math.round(performance.now() - startedAt),
        });
        return response;
      } catch (error) {
        const abortError = error && error.name === "AbortError";
        if (timedOut && abortError) {
          debugLogger.log("firmware-fetch-timeout", {
            url: requestUrl,
            durationMs: Math.round(performance.now() - startedAt),
          });
          throw new Error(
            "Timed out downloading firmware files. Check connection, VPN/proxy, and browser extensions, then retry."
          );
        }
        debugLogger.log("firmware-fetch-failed", {
          url: requestUrl,
          durationMs: Math.round(performance.now() - startedAt),
          error,
        });
        throw error;
      } finally {
        clearTimeout(timeoutId);
        if (upstreamAbortListener && upstreamSignal) {
          upstreamSignal.removeEventListener("abort", upstreamAbortListener);
        }
      }
    };
  }

  function detectBrowserSupport() {
    const hasWebSerial = "serial" in navigator;
    browserEl.textContent = hasWebSerial ? "Web Serial supported" : "Use Chrome/Edge (desktop)";
    debug.log("browser-support", { hasWebSerial });
  }

  function detectSecureContext() {
    secureEl.textContent = window.isSecureContext ? "Secure context OK" : "Must be served over HTTPS";
    debug.log("secure-context", { secure: window.isSecureContext });
  }

  function installSerialPortFilterGuard() {
    if (!("serial" in navigator) || !navigator.serial) return;
    if (typeof navigator.serial.requestPort !== "function") return;
    if (shouldDisableSerialFiltering()) {
      debug.log("serial-request-port-filter-disabled", {
        query: window.location.search,
        hint: "Serial port filtering disabled via query parameter.",
      });
      return;
    }

    const serial = navigator.serial;
    if (serial.__mrf2RequestPortWrapped) return;

    const originalRequestPort = serial.requestPort.bind(serial);

    try {
      serial.requestPort = async (options) => {
        const requestOptions = options && typeof options === "object" ? { ...options } : {};
        const hasFilters =
          Array.isArray(requestOptions.filters) && requestOptions.filters.length > 0;

        if (!hasFilters) {
          requestOptions.filters = DEFAULT_SERIAL_FILTERS;
          debug.log("serial-request-port-filter-applied", {
            filters: DEFAULT_SERIAL_FILTERS,
          });
        }

        const port = await originalRequestPort(requestOptions);
        try {
          const info = typeof port.getInfo === "function" ? port.getInfo() : {};
          const usbVendorId = Number(info.usbVendorId || 0);
          const usbProductId = Number(info.usbProductId || 0);
          debug.log("serial-port-selected", { usbVendorId, usbProductId });

          if (isAdafruitRuntimePort(usbVendorId, usbProductId) && !shouldAllowRuntimePortFlashing()) {
            const bootloaderPort = await trySwitchAdafruitRuntimeToBootloader(port, usbProductId);
            if (bootloaderPort) {
              const bootloaderInfo = bootloaderPort.getInfo();
              debug.log("serial-port-auto-switched", {
                fromUsbVendorId: usbVendorId,
                fromUsbProductId: usbProductId,
                toUsbVendorId: Number(bootloaderInfo.usbVendorId || 0),
                toUsbProductId: Number(bootloaderInfo.usbProductId || 0),
              });
              return bootloaderPort;
            }

            const expectedBootloaderPid = runtimePidToBootloaderPid(usbProductId);
            const message = `Selected Adafruit runtime port ${toHex16(usbVendorId)}:${toHex16(usbProductId)} and failed to auto-switch to bootloader ${toHex16(ADAFRUIT_USB_VENDOR_ID)}:${toHex16(expectedBootloaderPid)}. Retry and select the bootloader port, or use BOOT then RESET. Add ?allowruntime=1 to bypass runtime switching.`;
            debug.log("serial-port-auto-switch-failed", {
              usbVendorId,
              usbProductId,
              expectedBootloaderPid,
            });
            throw new Error(message);
          }
        } catch (error) {
          if (error instanceof Error && error.message.includes("Selected Adafruit runtime port")) {
            throw error;
          }
          debug.log("serial-port-info-check-failed", { error });
        }

        return port;
      };

      Object.defineProperty(serial, "__mrf2RequestPortWrapped", {
        configurable: false,
        enumerable: false,
        writable: false,
        value: true,
      });

      debug.log("serial-request-port-guard-installed", {
        filters: DEFAULT_SERIAL_FILTERS,
      });
    } catch (error) {
      debug.log("serial-request-port-guard-failed", { error });
    }
  }

  function normalizeManifestPath(manifestPath) {
    if (!manifestPath) return "";
    if (/^https?:\/\//i.test(manifestPath)) return manifestPath;
    const trimmedPath = manifestPath.replace(/^\/+/, "");
    if (!trimmedPath) return "";
    return trimmedPath.startsWith("./") ? trimmedPath : `./${trimmedPath}`;
  }

  function compareVersionsDescending(lhs, rhs) {
    return rhs.localeCompare(lhs, undefined, { numeric: true, sensitivity: "base" });
  }

  function normalizeVersionString(rawVersion) {
    if (typeof rawVersion !== "string") return "";
    const trimmed = rawVersion.trim();
    if (!trimmed) return "";
    const match = trimmed.match(/([0-9]+(?:\.[0-9]+)+)/);
    return match ? match[1] : "";
  }

  function inferVersionFromManifestPath(manifestPath) {
    if (!manifestPath || typeof manifestPath !== "string") return "";
    const match = manifestPath.match(/(?:^|\/)versions\/([^/]+)\/manifest\.json$/i);
    if (!match || !match[1]) return "";
    return normalizeVersionString(match[1]);
  }

  async function fetchManifest(manifestPath) {
    const response = await fetch(manifestPath, { cache: "no-store" });
    if (!response.ok) {
      throw new Error("Manifest not available");
    }
    return response.json();
  }

  function normalizeVersionEntry(entry) {
    if (!entry || typeof entry !== "object") return null;
    const rawVersion = typeof entry.version === "string" ? entry.version.trim() : "";
    const manifestPath =
      typeof entry.manifest === "string" && entry.manifest.trim()
        ? entry.manifest.trim()
        : rawVersion
          ? `firmware/versions/${rawVersion}/manifest.json`
          : "";
    const manifest = normalizeManifestPath(manifestPath);
    if (!manifest) return null;
    const version = normalizeVersionString(rawVersion) || inferVersionFromManifestPath(manifest);
    return { version, manifest };
  }

  function setInstallManifest(manifestPath) {
    if (!installBtnEl || !manifestPath) return;
    installBtnEl.setAttribute("manifest", manifestPath);
  }

  function renderUnavailableVersionOption(message) {
    if (!versionSelectEl) return;
    versionSelectEl.innerHTML = "";
    const option = document.createElement("option");
    option.value = FALLBACK_MANIFEST_PATH;
    option.textContent = message;
    versionSelectEl.appendChild(option);
    versionSelectEl.disabled = true;
  }

  function renderVersionOptions(entries, latestVersion) {
    if (!versionSelectEl) return;
    versionSelectEl.innerHTML = "";

    if (!entries.length) {
      const option = document.createElement("option");
      option.value = FALLBACK_MANIFEST_PATH;
      option.textContent = "No published builds";
      versionSelectEl.appendChild(option);
      versionSelectEl.disabled = true;
      return;
    }

    entries.forEach((entry) => {
      const option = document.createElement("option");
      option.value = entry.manifest;
      const baseLabel = entry.version || "Latest";
      option.textContent =
        latestVersion && entry.version === latestVersion ? `${baseLabel} (Latest)` : baseLabel;
      versionSelectEl.appendChild(option);
    });

    versionSelectEl.disabled = entries.length <= 1;
  }

  async function loadVersionCatalog() {
    debug.log("version-catalog-load-start", { path: VERSION_INDEX_PATH });
    try {
      const response = await fetch(VERSION_INDEX_PATH, { cache: "no-store" });
      if (!response.ok) {
        throw new Error("Version catalog not available");
      }

      const payload = await response.json();
      const entries = (Array.isArray(payload.versions) ? payload.versions : [])
        .map(normalizeVersionEntry)
        .filter((entry) => !!entry);

      if (!entries.length) {
        throw new Error("Version catalog is empty");
      }

      entries.sort((lhs, rhs) => compareVersionsDescending(lhs.version, rhs.version));

      let latestVersion =
        typeof payload.latest === "string" ? normalizeVersionString(payload.latest) : "";
      if (!latestVersion || !entries.some((entry) => entry.version === latestVersion)) {
        latestVersion = entries[0].version;
      }

      versionEl.textContent = latestVersion || "Available";
      debug.log("version-catalog-load-success", {
        latestVersion,
        versionCount: entries.length,
      });
      return { entries, latestVersion };
    } catch (error) {
      debug.log("version-catalog-load-failed", { error });
      return null;
    }
  }

  async function loadFallbackCatalog() {
    debug.log("manifest-load-start", { manifest: FALLBACK_MANIFEST_PATH });
    try {
      const manifest = await fetchManifest(FALLBACK_MANIFEST_PATH);
      const version =
        manifest && typeof manifest.version === "string"
          ? normalizeVersionString(manifest.version)
          : "";
      versionEl.textContent = version || "Available";
      debug.log("manifest-load-success", { version });
      return {
        entries: [{ version, manifest: FALLBACK_MANIFEST_PATH }],
        latestVersion: version,
      };
    } catch (error) {
      versionEl.textContent = "Not published yet";
      debug.log("manifest-load-failed", { error });
      return { entries: [], latestVersion: "" };
    }
  }

  async function initializeFirmwareCatalog() {
    try {
      const catalog = (await loadVersionCatalog()) || (await loadFallbackCatalog());
      const entries = catalog && Array.isArray(catalog.entries) ? catalog.entries : [];
      const latestVersion =
        catalog && typeof catalog.latestVersion === "string" ? catalog.latestVersion : "";

      renderVersionOptions(entries, latestVersion);

      if (!entries.length) {
        setInstallManifest(FALLBACK_MANIFEST_PATH);
        loadLatestChangelog("");
        return;
      }

      const findEntryByManifest = (manifestPath) =>
        entries.find((entry) => entry.manifest === manifestPath) || null;

      const applySelection = (entry, source) => {
        if (!entry) return;
        setInstallManifest(entry.manifest);
        if (versionSelectEl) {
          versionSelectEl.value = entry.manifest;
        }
        loadLatestChangelog(entry.version);
        debug.log("firmware-selection", {
          source,
          version: entry.version || "",
          manifest: entry.manifest,
        });
      };

      const defaultEntry =
        entries.find((entry) => latestVersion && entry.version === latestVersion) || entries[0];
      applySelection(defaultEntry, "default");

      if (versionSelectEl) {
        versionSelectEl.addEventListener("change", () => {
          const selectedEntry = findEntryByManifest(versionSelectEl.value);
          if (selectedEntry) {
            applySelection(selectedEntry, "user");
          }
        });
      }
    } catch (error) {
      debug.log("firmware-catalog-init-failed", { error });
      versionEl.textContent = "Available";
      renderUnavailableVersionOption("Version list unavailable");
      setInstallManifest(FALLBACK_MANIFEST_PATH);
      loadLatestChangelog("");
    }
  }

  function escapeRegex(value) {
    return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  }

  function extractChangelogBlocksFromSection(sectionBody) {
    const blocks = [];
    let listRootItems = null;
    let listStack = null;

    const flushListBlock = () => {
      if (!Array.isArray(listRootItems) || !listRootItems.length) {
        listRootItems = null;
        listStack = null;
        return;
      }

      blocks.push({ type: "list", items: listRootItems });
      listRootItems = null;
      listStack = null;
    };

    const ensureListContext = () => {
      if (!Array.isArray(listRootItems)) {
        listRootItems = [];
        listStack = [{ indent: -1, items: listRootItems }];
      }
      return listStack;
    };

    const appendParagraphBlock = (text) => {
      const normalized = (text || "").trim();
      if (!normalized) return;

      const lastBlock = blocks.length ? blocks[blocks.length - 1] : null;
      if (lastBlock && lastBlock.type === "paragraph") {
        lastBlock.text = `${lastBlock.text} ${normalized}`;
        return;
      }

      blocks.push({ type: "paragraph", text: normalized });
    };

    sectionBody.split(/\r?\n/).forEach((rawLine) => {
      const bulletMatch = rawLine.match(/^(\s*)-\s+(.*)$/);
      if (bulletMatch) {
        const noteText = (bulletMatch[2] || "").trim();
        if (
          !noteText ||
          noteText.startsWith("Release commit:") ||
          noteText.startsWith("Range:")
        ) {
          return;
        }

        const stack = ensureListContext();
        const indentWidth = (bulletMatch[1] || "").replace(/\t/g, "  ").length;
        while (stack.length > 1 && indentWidth <= stack[stack.length - 1].indent) {
          stack.pop();
        }

        const item = { text: noteText, children: [] };
        stack[stack.length - 1].items.push(item);
        stack.push({ indent: indentWidth, items: item.children });
        return;
      }

      flushListBlock();
      const trimmedLine = rawLine.trim();
      if (!trimmedLine) return;

      const headingMatch = trimmedLine.match(/^#{3,6}\s+(.*)$/);
      if (headingMatch && headingMatch[1]) {
        blocks.push({ type: "heading", text: headingMatch[1].trim() });
        return;
      }

      appendParagraphBlock(trimmedLine);
    });

    flushListBlock();
    return blocks;
  }

  function countChangelogNotes(notes) {
    if (!Array.isArray(notes)) return 0;

    return notes.reduce((sum, note) => {
      const nestedCount = countChangelogNotes(note && note.children);
      return sum + 1 + nestedCount;
    }, 0);
  }

  function countChangelogContentItems(blocks) {
    if (!Array.isArray(blocks)) return 0;

    return blocks.reduce((sum, block) => {
      if (!block || typeof block.type !== "string") return sum;

      if (block.type === "list") {
        return sum + countChangelogNotes(block.items);
      }

      if (
        (block.type === "heading" || block.type === "paragraph") &&
        typeof block.text === "string" &&
        block.text.length
      ) {
        return sum + 1;
      }

      return sum;
    }, 0);
  }

  function appendChangelogNotes(listEl, notes) {
    if (!listEl || !Array.isArray(notes) || !notes.length) return;

    notes.forEach((note) => {
      if (!note || typeof note.text !== "string" || !note.text.length) return;

      const li = document.createElement("li");
      appendInlineCodeSpans(li, note.text);
      listEl.appendChild(li);

      if (Array.isArray(note.children) && note.children.length) {
        const nestedList = document.createElement("ul");
        appendChangelogNotes(nestedList, note.children);
        li.appendChild(nestedList);
      }
    });
  }

  function appendInlineCodeSpans(containerEl, text) {
    if (!containerEl || typeof text !== "string" || !text.length) return;

    const parts = text.split(/(`[^`]+`)/g);
    parts.forEach((part) => {
      if (!part) return;

      const isCodeSpan = part.length >= 3 && part.startsWith("`") && part.endsWith("`");
      if (isCodeSpan) {
        const code = document.createElement("code");
        code.textContent = part.slice(1, -1);
        containerEl.appendChild(code);
        return;
      }

      containerEl.appendChild(document.createTextNode(part));
    });
  }

  function appendChangelogSectionBlocks(sectionItem, blocks) {
    if (!sectionItem || !Array.isArray(blocks) || !blocks.length) return false;

    let appendedAny = false;

    blocks.forEach((block) => {
      if (!block || typeof block.type !== "string") return;

      if (block.type === "list" && Array.isArray(block.items) && block.items.length) {
        const sectionList = document.createElement("ul");
        sectionList.className = "release-section-list";
        appendChangelogNotes(sectionList, block.items);
        if (sectionList.childElementCount) {
          sectionItem.appendChild(sectionList);
          appendedAny = true;
        }
        return;
      }

      if (
        (block.type === "heading" || block.type === "paragraph") &&
        typeof block.text === "string" &&
        block.text.length
      ) {
        const textBlock = document.createElement("p");
        textBlock.className =
          block.type === "heading" ? "release-section-subheading" : "release-section-text";
        appendInlineCodeSpans(textBlock, block.text);
        sectionItem.appendChild(textBlock);
        appendedAny = true;
      }
    });

    return appendedAny;
  }

  function extractChangelogSections(markdown) {
    if (!markdown) return [];

    const sectionHeadingRegex = /^##\s+([^\s]+)\s+-.*$/gm;
    const sections = [];
    let headingMatch = null;

    while ((headingMatch = sectionHeadingRegex.exec(markdown)) !== null) {
      sections.push({
        version: headingMatch[1],
        sectionStart: headingMatch.index,
        sectionBodyStart: sectionHeadingRegex.lastIndex,
      });
    }

    if (!sections.length) return [];

    return sections.map((section, index) => {
      const nextSectionStart =
        index + 1 < sections.length ? sections[index + 1].sectionStart : markdown.length;
      const sectionBody = markdown.slice(section.sectionBodyStart, nextSectionStart);
      return {
        version: section.version,
        blocks: extractChangelogBlocksFromSection(sectionBody),
      };
    });
  }

  function selectCurrentAndPreviousSections(changelogSections, version) {
    if (!Array.isArray(changelogSections) || !changelogSections.length) return [];

    let currentSectionIndex = 0;
    if (version) {
      const selectedIndex = changelogSections.findIndex((section) => section.version === version);
      if (selectedIndex >= 0) {
        currentSectionIndex = selectedIndex;
      }
    }

    const selectedSections = [changelogSections[currentSectionIndex]];
    if (currentSectionIndex + 1 < changelogSections.length) {
      selectedSections.push(changelogSections[currentSectionIndex + 1]);
    }

    return selectedSections;
  }

  function renderChangelogNotes(sections) {
    if (!latestChangelogEl) return;
    latestChangelogEl.innerHTML = "";

    let renderedSectionCount = 0;

    sections.forEach((section) => {
      if (!section || !Array.isArray(section.blocks) || !section.blocks.length) {
        return;
      }

      const sectionItem = document.createElement("li");
      sectionItem.className = "release-section";

      const sectionHeading = document.createElement("p");
      sectionHeading.className = "release-section-heading";
      appendInlineCodeSpans(sectionHeading, `Release ${section.version}`);
      sectionItem.appendChild(sectionHeading);

      const hasRenderedBlocks = appendChangelogSectionBlocks(sectionItem, section.blocks);
      if (!hasRenderedBlocks) return;

      latestChangelogEl.appendChild(sectionItem);
      renderedSectionCount += 1;
    });

    if (renderedSectionCount > 0) return;

    const li = document.createElement("li");
    li.className = "release-empty";
    appendInlineCodeSpans(li, "Release notes not available yet.");
    latestChangelogEl.appendChild(li);
  }

  async function loadLatestChangelog(version) {
    if (changelogLinkEl) {
      changelogLinkEl.href = "./changelog.md";
    }

    try {
      const response = await fetch("./changelog.md", { cache: "no-store" });
      if (!response.ok) {
        throw new Error("Changelog not available");
      }
      const changelog = await response.text();
      const changelogSections = extractChangelogSections(changelog);
      const sectionsToRender = selectCurrentAndPreviousSections(changelogSections, version);
      renderChangelogNotes(sectionsToRender);
      const noteCount = sectionsToRender.reduce(
        (sum, section) => sum + countChangelogContentItems(section.blocks),
        0
      );
      debug.log("changelog-load-success", {
        version,
        sectionCount: sectionsToRender.length,
        noteCount,
      });
    } catch (error) {
      renderChangelogNotes([]);
      debug.log("changelog-load-failed", { version, error });
    }
  }

  function patchInstallSuccessMessage() {
    const installCompleteText = "Installation complete!";
    const installCompleteWithReboot = "Installation complete! Reboot the camera.";
    const observedDialogs = new WeakSet();

    const updateDialogMessage = (dialogEl) => {
      if (!dialogEl || !dialogEl.shadowRoot) return;
      const successMessage = dialogEl.shadowRoot.querySelector(
        'ewt-page-message[label="Installation complete!"]'
      );
      if (successMessage) {
        successMessage.setAttribute("label", installCompleteWithReboot);
      }
    };

    const observer = new MutationObserver(() => {
      const dialogEl = document.querySelector("ewt-install-dialog");
      if (!dialogEl || !dialogEl.shadowRoot) return;

      updateDialogMessage(dialogEl);
      if (observedDialogs.has(dialogEl)) return;
      observedDialogs.add(dialogEl);

      const dialogObserver = new MutationObserver(() => {
        const successMessage = dialogEl.shadowRoot.querySelector("ewt-page-message");
        if (!successMessage) return;
        if (successMessage.getAttribute("label") === installCompleteText) {
          successMessage.setAttribute("label", installCompleteWithReboot);
        }
      });

      dialogObserver.observe(dialogEl.shadowRoot, {
        childList: true,
        subtree: true,
        attributes: true,
        attributeFilter: ["label"],
      });
    });

    observer.observe(document.body, { childList: true, subtree: true });
  }

  function patchInstallErrorHints() {
    const observedDialogs = new WeakSet();
    const knownMessages = [
      "The device has been lost.",
      "Failed to initialize. Try resetting your device or holding the BOOT button while clicking INSTALL.",
    ];
    const guidance =
      "Connection lost. Put the camera in ESP download mode (hold BOOT, tap RESET, release BOOT), select the ESP32-S3 port, and retry.";

    const patchDialogErrors = (dialogEl) => {
      if (!dialogEl || !dialogEl.shadowRoot) return;
      const messageEls = dialogEl.shadowRoot.querySelectorAll("ewt-page-message");
      if (!messageEls.length) return;

      messageEls.forEach((messageEl) => {
        const label = messageEl.getAttribute("label") || "";
        if (!knownMessages.includes(label)) return;
        messageEl.setAttribute("label", guidance);
      });
    };

    const observer = new MutationObserver(() => {
      const dialogEl = document.querySelector("ewt-install-dialog");
      if (!dialogEl || !dialogEl.shadowRoot) return;

      patchDialogErrors(dialogEl);
      if (observedDialogs.has(dialogEl)) return;
      observedDialogs.add(dialogEl);

      const dialogObserver = new MutationObserver(() => {
        patchDialogErrors(dialogEl);
      });

      dialogObserver.observe(dialogEl.shadowRoot, {
        childList: true,
        subtree: true,
        attributes: true,
        attributeFilter: ["label"],
      });
    });

    observer.observe(document.body, { childList: true, subtree: true });
  }

  if (installBtnEl) {
    installBtnEl.addEventListener("click", () => {
      debug.log("install-button-click", {
        manifest: installBtnEl.getAttribute("manifest") || "",
      });
    });
  }

  installFirmwareFetchTimeoutGuard(debug);
  detectBrowserSupport();
  detectSecureContext();
  installSerialPortFilterGuard();
  initializeFirmwareCatalog();
  patchInstallSuccessMessage();
  patchInstallErrorHints();
})();
