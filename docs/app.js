(function () {
  const versionEl = document.getElementById("firmware-version");
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

  function installFirmwareFetchTimeoutGuard(debugLogger) {
    if (typeof window.fetch !== "function") return;
    const originalFetch = window.fetch.bind(window);

    const isFirmwareArtifactRequest = (input) => {
      try {
        const rawUrl = typeof input === "string" ? input : input && input.url;
        if (!rawUrl) return false;
        const url = new URL(rawUrl, window.location.href);
        if (!url.pathname.includes("/firmware/latest/")) return false;
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

  async function loadManifestVersion() {
    debug.log("manifest-load-start");
    try {
      const response = await fetch("./firmware/latest/manifest.json", { cache: "no-store" });
      if (!response.ok) {
        throw new Error("Manifest not available yet");
      }

      const manifest = await response.json();
      if (manifest && manifest.version) {
        versionEl.textContent = manifest.version;
        debug.log("manifest-load-success", { version: manifest.version });
        return manifest.version;
      } else {
        versionEl.textContent = "Available";
        debug.log("manifest-load-success", { version: "" });
      }
    } catch (error) {
      versionEl.textContent = "Not published yet";
      debug.log("manifest-load-failed", { error });
    }
    return "";
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
      li.textContent = "Latest release notes not available yet.";
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

  function autoRetryTransientInstallFailure() {
    const RETRY_DELAY_MS = 120;
    const DIALOG_RESHOW_DELAY_MS = 220;
    const watchedDialogs = new WeakSet();
    const retriedDialogs = new WeakSet();
    const hiddenDialogs = new WeakSet();
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

    const maybeRetry = (dialogEl) => {
      if (!dialogEl || retriedDialogs.has(dialogEl)) return;
      logInstallStateIfChanged(dialogEl, "maybe-retry");
      if (!hasRetryableInitializeFailure(dialogEl)) return;

      retriedDialogs.add(dialogEl);
      debug.log("install-auto-retry-start");
      // Hide the first transient initialize failure while we auto-retry.
      hideDialog(dialogEl);
      dialogEl._installState = {
        state: "initializing",
        message: "Retrying initialization...",
        details: { done: false, autoRetry: true },
      };
      logInstallStateIfChanged(dialogEl, "auto-retry-set-state");
      if (typeof dialogEl.requestUpdate === "function") {
        dialogEl.requestUpdate();
      }
      setTimeout(() => {
        try {
          if (typeof dialogEl._confirmInstall === "function") {
            const retryResult = dialogEl._confirmInstall();
            debug.log("install-auto-retry-dispatched");
            if (retryResult && typeof retryResult.catch === "function") {
              retryResult.catch((error) => {
                console.warn("Automatic install retry failed", error);
                debug.log("install-auto-retry-promise-failed", { error });
              });
            }
          }
        } catch (error) {
          console.warn("Automatic install retry failed", error);
          debug.log("install-auto-retry-throw", { error });
        } finally {
          setTimeout(() => {
            showDialog(dialogEl);
            logInstallStateIfChanged(dialogEl, "dialog-reshow");
          }, DIALOG_RESHOW_DELAY_MS);
        }
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
      if (!dialogEl) return;
      debug.log("install-dialog-detected");
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
      debug.log("install-button-click");
    });
  }

  installFirmwareFetchTimeoutGuard(debug);
  detectBrowserSupport();
  detectSecureContext();
  loadManifestVersion().then((version) => {
    loadLatestChangelog(version);
  });
  patchInstallSuccessMessage();
  autoRetryTransientInstallFailure();
})();
