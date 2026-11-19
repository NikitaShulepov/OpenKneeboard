class VDXROVRSimHubHooks {
  constructor() {
    console.log("VDXROVRSimHubHooks::Constructor()");
    window.addEventListener(
      "onmainsimhubdashmetadata",
      this.#onMetadata.bind(this));
  }

  async #onMetadata(ev) {
    console.log("VDXROVR: SimHub metadata hook");
    const width = ev.detail.Width;
    const height = ev.detail.Height;

    console.log(`VDXROVR: Requesting resize to ${width}x${height}`);

    try {
      const result = await window.VDXROVR.SetPreferredPixelSize(width, height);
      console.log("VDXROVR: resized to fit SimHub template", result);
    } catch (error) {
      console.log("VDXROVR: failed to resize", error);
    }
  }
}

window.VDXROVR.SimHubHooks = new VDXROVRSimHubHooks();