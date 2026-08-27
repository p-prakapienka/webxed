export class SysexLoader {
    constructor(api, session, patchBrowser, status) {
        this.api = api;
        this.session = session;
        this.patchBrowser = patchBrowser;
        this.status = status;
    }

    async load(file) {
        const bytes = new Uint8Array(await file.arrayBuffer());
        const count = this.api.loadSysex(this.session, bytes);
        if (count < 1) {
            this.status.textContent = 'Could not parse this DX7 SysEx file.';
            return false;
        }

        this.patchBrowser.load(count);
        this.status.textContent = `Loaded ${file.name}: ${count} patch${count === 1 ? '' : 'es'}.`;
        return true;
    }
}
