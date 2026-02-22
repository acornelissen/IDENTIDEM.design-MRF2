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
  const AUTO_RETRY_DISABLE_QUERY_KEY = "noretry";
  const AUTO_RETRY_QUERY_KEY = "retry";
  const SERIAL_FILTER_DISABLE_QUERY_KEY = "allports";
  const SERIAL_FILTER_DISABLE_QUERY_KEY_ALT = "nofilter";
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

  function parseBooleanQueryValue(value) {
    const normalized = (value || "").trim().toLowerCase();
    if (["1", "true", "yes", "on"].includes(normalized)) return true;
    if (["0", "false", "no", "off"].includes(normalized)) return false;
    return null;
  }

  function shouldDisableAutoRetry() {
    const queryParams = new URLSearchParams(window.location.search);
    const disableValue = parseBooleanQueryValue(queryParams.get(AUTO_RETRY_DISABLE_QUERY_KEY));
    if (disableValue === true) return true;

    const retryValue = parseBooleanQueryValue(queryParams.get(AUTO_RETRY_QUERY_KEY));
    if (retryValue === true) return false;

    // Default: keep auto-retry off unless explicitly enabled with ?retry=1.
    return true;
  }

  function shouldDisableSerialFiltering() {
    const queryParams = new URLSearchParams(window.location.search);
    const disableValue = parseBooleanQueryValue(
      queryParams.get(SERIAL_FILTER_DISABLE_QUERY_KEY)
    );
    if (disableValue === true) return true;

    const disableAltValue = parseBooleanQueryValue(
      queryParams.get(SERIAL_FILTER_DISABLE_QUERY_KEY_ALT)
    );
    return disableAltValue === true;
  }

  function shouldAllowRuntimePortFlashing() {
    const queryParams = new URLSearchParams(window.location.search);
    const allowValue = parseBooleanQueryValue(queryParams.get(ALLOW_RUNTIME_PORT_QUERY_KEY));
    return allowValue === true;
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
    const queryParams = new URLSearchParams(window.location.search);
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
  const autoRetryDisabled = shouldDisableAutoRetry();

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

  async function fetchManifest(manifestPath) {
    const response = await fetch(manifestPath, { cache: "no-store" });
    if (!response.ok) {
      throw new Error("Manifest not available");
    }
    return response.json();
  }

  function normalizeVersionEntry(entry) {
    if (!entry || typeof entry !== "object") return null;
    const version = typeof entry.version === "string" ? entry.version.trim() : "";
    const manifestPath =
      typeof entry.manifest === "string" && entry.manifest.trim()
        ? entry.manifest.trim()
        : version
          ? `firmware/versions/${version}/manifest.json`
          : "";
    const manifest = normalizeManifestPath(manifestPath);
    if (!manifest) return null;
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

      let latestVersion = typeof payload.latest === "string" ? payload.latest.trim() : "";
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
        manifest && typeof manifest.version === "string" ? manifest.version.trim() : "";
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

  function extractChangelogNotes(markdown, version) {
    if (!markdown || !version) return [];
    const sectionPattern = new RegExp(`^##\\s+${escapeRegex(version)}\\s+-.*$`, "m");
    const sectionMatch = markdown.match(sectionPattern);
    if (!sectionMatch || typeof sectionMatch.index !== "number") return [];

    const sectionStart = sectionMatch.index + sectionMatch[0].length;
    const remaining = markdown.slice(sectionStart);
    const nextHeadingIndex = remaining.search(/^##\s+/m);
    const sectionBody = nextHeadingIndex >= 0 ? remaining.slice(0, nextHeadingIndex) : remaining;

    return sectionBody
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter((line) => line.startsWith("- "))
      .map((line) => line.slice(2).trim())
      .filter(
        (line) =>
          line.length > 0 && !line.startsWith("Release commit:") && !line.startsWith("Range:")
      );
  }

  function renderChangelogNotes(notes) {
    if (!latestChangelogEl) return;
    latestChangelogEl.innerHTML = "";

    if (!notes.length) {
      const li = document.createElement("li");
      li.textContent = "Release notes not available yet.";
      latestChangelogEl.appendChild(li);
      return;
    }

    notes.slice(0, 8).forEach((note) => {
      const li = document.createElement("li");
      li.textContent = note;
      latestChangelogEl.appendChild(li);
    });
  }

  async function loadLatestChangelog(version) {
    if (changelogLinkEl) {
      changelogLinkEl.href = "./changelog.md";
    }
    if (!version) {
      renderChangelogNotes([]);
      debug.log("changelog-skip", { reason: "missing-version" });
      return;
    }

    try {
      const response = await fetch("./changelog.md", { cache: "no-store" });
      if (!response.ok) {
        throw new Error("Changelog not available");
      }
      const changelog = await response.text();
      const notes = extractChangelogNotes(changelog, version);
      renderChangelogNotes(notes);
      debug.log("changelog-load-success", { version, noteCount: notes.length });
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

  function autoRetryTransientInstallFailure() {
    const RETRY_DELAY_MS = 420;
    const DIALOG_RESHOW_DELAY_MS = 220;
    const watchedDialogs = new WeakSet();
    const retriedDialogs = new WeakSet();
    const hiddenDialogs = new WeakSet();
    let lastSeenDialog = null;
    const stateSignatures = new WeakMap();

    const hideDialog = (dialogEl) => {
      if (!dialogEl || hiddenDialogs.has(dialogEl)) return;
      dialogEl.style.visibility = "hidden";
      hiddenDialogs.add(dialogEl);
      debug.log("install-dialog-hidden");
    };

    const showDialog = (dialogEl) => {
      if (!dialogEl || !hiddenDialogs.has(dialogEl)) return;
      dialogEl.style.visibility = "";
      hiddenDialogs.delete(dialogEl);
      debug.log("install-dialog-shown");
    };

    const buildStateSummary = (dialogEl) => {
      if (!dialogEl || !dialogEl._installState) {
        return { state: "missing" };
      }

      const installState = dialogEl._installState;
      const details =
        installState && installState.details && typeof installState.details === "object"
          ? installState.details
          : {};

      return {
        state: typeof installState.state === "string" ? installState.state : "",
        message: typeof installState.message === "string" ? installState.message : "",
        error: typeof details.error === "string" ? details.error : "",
        done: typeof details.done === "boolean" ? details.done : null,
        autoRetry: !!details.autoRetry,
      };
    };

    const logInstallStateIfChanged = (dialogEl, source) => {
      const summary = buildStateSummary(dialogEl);
      const signature = JSON.stringify(summary);
      if (stateSignatures.get(dialogEl) === signature) return;
      stateSignatures.set(dialogEl, signature);
      debug.log("install-state", { source, ...summary });
    };

    const hasRetryableInitializeFailure = (dialogEl) => {
      if (!dialogEl) return false;
      const installState = dialogEl._installState;
      if (!installState || installState.state !== "error") return false;

      const errorCode =
        installState.details && typeof installState.details.error === "string"
          ? installState.details.error
          : "";
      const message = typeof installState.message === "string" ? installState.message : "";
      const normalizedMessage = message.toLowerCase();

      return errorCode === "failed_initialize" || normalizedMessage.includes("failed to initialize");
    };

    const cloneInstallState = (installState) => {
      if (!installState || typeof installState !== "object") return installState;
      try {
        return JSON.parse(JSON.stringify(installState));
      } catch (error) {
        return installState;
      }
    };

    const maybeOpenPortForRetry = async (dialogEl) => {
      if (!dialogEl || !dialogEl.port) return;
      const port = dialogEl.port;
      if (port.readable && port.writable) {
        debug.log("install-auto-retry-port-ready");
        return;
      }
      try {
        await port.open({ baudRate: 115200, bufferSize: 8192 });
        debug.log("install-auto-retry-port-opened");
      } catch (error) {
        debug.log("install-auto-retry-port-open-failed", { error });
      }
    };

    const maybeRetry = (dialogEl) => {
      if (!dialogEl || retriedDialogs.has(dialogEl)) return;
      logInstallStateIfChanged(dialogEl, "maybe-retry");
      if (!hasRetryableInitializeFailure(dialogEl)) return;

      retriedDialogs.add(dialogEl);
      debug.log("install-auto-retry-start");
      const previousInstallState = cloneInstallState(dialogEl._installState);

      const failRetry = (error) => {
        console.warn("Automatic install retry failed", error);
        debug.log("install-auto-retry-failed", { error });
        if (previousInstallState) {
          dialogEl._installState = previousInstallState;
        } else {
          dialogEl._installState = {
            state: "error",
            message: "Automatic retry failed. Please close and retry install.",
            details: { error: "auto_retry_failed" },
          };
        }
        if (typeof dialogEl.requestUpdate === "function") {
          dialogEl.requestUpdate();
        }
        showDialog(dialogEl);
        logInstallStateIfChanged(dialogEl, "auto-retry-failed");
      };

      // Hide the first transient initialize failure while we auto-retry.
      hideDialog(dialogEl);
      setTimeout(() => {
        (async () => {
          await maybeOpenPortForRetry(dialogEl);
          if (typeof dialogEl._confirmInstall === "function") {
            const retryResult = dialogEl._confirmInstall();
            debug.log("install-auto-retry-dispatched");
            if (retryResult && typeof retryResult.catch === "function") {
              retryResult.catch((error) => {
                failRetry(error);
              });
            }
          } else {
            throw new Error("Install retry entry point is missing");
          }
        })()
          .catch((error) => {
            failRetry(error);
          })
          .finally(() => {
            setTimeout(() => {
              showDialog(dialogEl);
              logInstallStateIfChanged(dialogEl, "dialog-reshow");
            }, DIALOG_RESHOW_DELAY_MS);
          });
      }, RETRY_DELAY_MS);
    };

    const watchDialog = (dialogEl) => {
      if (!dialogEl) return false;
      const shadowRoot = dialogEl.shadowRoot;
      if (!shadowRoot) return false;
      if (watchedDialogs.has(dialogEl)) return true;
      watchedDialogs.add(dialogEl);

      const dialogObserver = new MutationObserver(() => {
        logInstallStateIfChanged(dialogEl, "dialog-mutation");
        maybeRetry(dialogEl);
      });

      dialogObserver.observe(shadowRoot, {
        childList: true,
        subtree: true,
        attributes: true,
      });
      logInstallStateIfChanged(dialogEl, "watch-dialog");
      maybeRetry(dialogEl);
      return true;
    };

    const observer = new MutationObserver(() => {
      const dialogEl = document.querySelector("ewt-install-dialog");
      if (!dialogEl) {
        if (lastSeenDialog) {
          debug.log("install-dialog-removed");
          lastSeenDialog = null;
        }
        return;
      }
      if (dialogEl !== lastSeenDialog) {
        debug.log("install-dialog-detected");
        lastSeenDialog = dialogEl;
      }
      if (!watchDialog(dialogEl)) {
        requestAnimationFrame(() => {
          watchDialog(dialogEl);
        });
      }
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
  if (autoRetryDisabled) {
    debug.log("install-auto-retry-disabled", {
      hint: "Auto-retry is disabled by default. Pass ?retry=1 to enable it.",
      query: window.location.search,
    });
  } else {
    autoRetryTransientInstallFailure();
  }
})();
