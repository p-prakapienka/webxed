export class WebxedApi {
    constructor(module) {
        this.module = module;
        this.createSynth = module.cwrap('createSynth', 'number', ['number']);
        this.destroySynth = module.cwrap('destroySynth', null, ['number']);
        this.loadSysexNative = module.cwrap('loadSysex', 'number', ['number', 'number', 'number']);
        this.patchNameNative = module.cwrap('patchName', 'string', ['number', 'number']);
        this.selectPatchNative = module.cwrap('selectPatch', 'number', ['number', 'number']);
        this.selectPreviewEngineNative = module.cwrap('selectPreviewEngine', 'number', ['number', 'number']);
        this.noteOnNative = module.cwrap('noteOn', null, ['number', 'number', 'number']);
        this.noteOffNative = module.cwrap('noteOff', null, ['number']);
        this.renderSampleNative = module.cwrap('renderSample', 'number', ['number']);
    }

    createSession(sampleRate) {
        return this.createSynth(sampleRate);
    }

    destroySession(session) {
        this.destroySynth(session);
    }

    loadSysex(session, bytes) {
        const pointer = this.module._malloc(bytes.length);
        try {
            this.module.HEAPU8.set(bytes, pointer);
            return this.loadSysexNative(session, pointer, bytes.length);
        } finally {
            this.module._free(pointer);
        }
    }

    patchName(session, index) {
        return this.patchNameNative(session, index);
    }

    selectPatch(session, index) {
        return this.selectPatchNative(session, index) === 1;
    }

    selectPreviewEngine(session, engineIndex) {
        return this.selectPreviewEngineNative(session, engineIndex) === 1;
    }

    noteOn(session, midiNote, velocity) {
        this.noteOnNative(session, midiNote, velocity);
    }

    noteOff(session) {
        this.noteOffNative(session);
    }

    renderSample(session) {
        return this.renderSampleNative(session);
    }
}
