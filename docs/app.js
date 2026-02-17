(function () {
  const versionEl = document.getElementById("firmware-version");
  const browserEl = document.getElementById("browser-check");
  const secureEl = document.getElementById("secure-check");
  const latestChangelogEl = document.getElementById("latest-changelog");
  const changelogLinkEl = document.getElementById("changelog-link");
  const FIRMWARE_FETCH_TIMEOUT_MS = 30000;

  function installFirmwareFetchTimeoutGuard() {
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

    window.fetch = async (input, init) => {
      if (!isFirmwareArtifactRequest(input)) {
        return originalFetch(input, init);
      }

      const controller = new AbortController();
      const upstreamSignal = init && init.signal;
      let timedOut = false;
      let upstreamAbortListener = null;

      if (upstreamSignal && typeof upstreamSignal.addEventListener === "function") {
        if (upstreamSignal.aborted) {
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
        return response;
      } catch (error) {
        const abortError = error && error.name === "AbortError";
        if (timedOut && abortError) {
          throw new Error(
            "Timed out downloading firmware files. Check connection, VPN/proxy, and browser extensions, then retry."
          );
        }
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
  }

  function detectSecureContext() {
    secureEl.textContent = window.isSecureContext ? "Secure context OK" : "Must be served over HTTPS";
  }

  async function loadManifestVersion() {
    try {
      const response = await fetch("./firmware/latest/manifest.json", { cache: "no-store" });
      if (!response.ok) {
        throw new Error("Manifest not available yet");
      }

      const manifest = await response.json();
      if (manifest && manifest.version) {
        versionEl.textContent = manifest.version;
        return manifest.version;
      } else {
        versionEl.textContent = "Available";
      }
    } catch (error) {
      versionEl.textContent = "Not published yet";
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
    } catch (error) {
      renderChangelogNotes([]);
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

    const hideDialog = (dialogEl) => {
      if (!dialogEl || hiddenDialogs.has(dialogEl)) return;
      dialogEl.style.visibility = "hidden";
      hiddenDialogs.add(dialogEl);
    };

    const showDialog = (dialogEl) => {
      if (!dialogEl || !hiddenDialogs.has(dialogEl)) return;
      dialogEl.style.visibility = "";
      hiddenDialogs.delete(dialogEl);
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
      if (!hasRetryableInitializeFailure(dialogEl)) return;

      retriedDialogs.add(dialogEl);
      // Hide the first transient initialize failure while we auto-retry.
      hideDialog(dialogEl);
      dialogEl._installState = {
        state: "initializing",
        message: "Retrying initialization...",
        details: { done: false, autoRetry: true },
      };
      if (typeof dialogEl.requestUpdate === "function") {
        dialogEl.requestUpdate();
      }
      setTimeout(() => {
        try {
          if (typeof dialogEl._confirmInstall === "function") {
            const retryResult = dialogEl._confirmInstall();
            if (retryResult && typeof retryResult.catch === "function") {
              retryResult.catch((error) => {
                console.warn("Automatic install retry failed", error);
              });
            }
          }
        } catch (error) {
          console.warn("Automatic install retry failed", error);
        } finally {
          setTimeout(() => {
            showDialog(dialogEl);
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
        maybeRetry(dialogEl);
      });

      dialogObserver.observe(shadowRoot, {
        childList: true,
        subtree: true,
        attributes: true,
      });
      maybeRetry(dialogEl);
      return true;
    };

    const observer = new MutationObserver(() => {
      const dialogEl = document.querySelector("ewt-install-dialog");
      if (!dialogEl) return;
      if (!watchDialog(dialogEl)) {
        requestAnimationFrame(() => {
          watchDialog(dialogEl);
        });
      }
    });

    observer.observe(document.body, { childList: true, subtree: true });
  }

  installFirmwareFetchTimeoutGuard();
  detectBrowserSupport();
  detectSecureContext();
  loadManifestVersion().then((version) => {
    loadLatestChangelog(version);
  });
  patchInstallSuccessMessage();
  autoRetryTransientInstallFailure();
})();
