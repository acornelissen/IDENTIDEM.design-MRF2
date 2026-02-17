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

  detectBrowserSupport();
  detectSecureContext();
  loadManifestVersion();
})();
