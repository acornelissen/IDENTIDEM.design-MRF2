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

  detectBrowserSupport();
  detectSecureContext();
  loadManifestVersion();
  patchInstallSuccessMessage();
})();
