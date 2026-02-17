(function () {
  const versionEl = document.getElementById("firmware-version");
  const browserEl = document.getElementById("browser-check");
  const secureEl = document.getElementById("secure-check");

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
      } else {
        versionEl.textContent = "Available";
      }
    } catch (error) {
      versionEl.textContent = "Not published yet";
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
    const RETRY_DELAY_MS = 900;
    const DIALOG_RESHOW_DELAY_MS = 200;
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

    const maybeRetry = (dialogEl) => {
      if (!dialogEl || retriedDialogs.has(dialogEl)) return;

      const installState = dialogEl._installState;
      const isRetryableInitializeFailure =
        installState &&
        installState.state === "error" &&
        installState.details &&
        installState.details.error === "failed_initialize";

      if (!isRetryableInitializeFailure) return;

      retriedDialogs.add(dialogEl);
      // Hide the first transient initialize failure while we auto-retry.
      hideDialog(dialogEl);
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
      if (!dialogEl || watchedDialogs.has(dialogEl)) return;
      watchedDialogs.add(dialogEl);

      const shadowRoot = dialogEl.shadowRoot;
      if (!shadowRoot) return;

      const dialogObserver = new MutationObserver(() => {
        maybeRetry(dialogEl);
      });

      dialogObserver.observe(shadowRoot, {
        childList: true,
        subtree: true,
        attributes: true,
      });
      maybeRetry(dialogEl);
    };

    const observer = new MutationObserver(() => {
      const dialogEl = document.querySelector("ewt-install-dialog");
      if (!dialogEl) return;
      watchDialog(dialogEl);
    });

    observer.observe(document.body, { childList: true, subtree: true });
  }

  detectBrowserSupport();
  detectSecureContext();
  loadManifestVersion();
  patchInstallSuccessMessage();
  autoRetryTransientInstallFailure();
})();
