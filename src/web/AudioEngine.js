export class AudioEngine {
    constructor(api, session, audioContext) {
        this.api = api;
        this.session = session;
        this.audioContext = audioContext;
        this.processor = audioContext.createScriptProcessor(512, 0, 1);
        this.noteActive = false;

        this.processor.onaudioprocess = event => {
            const output = event.outputBuffer.getChannelData(0);
            for (let index = 0; index < output.length; index += 1) {
                output[index] = this.api.renderSample(this.session);
            }
        };
        this.processor.connect(audioContext.destination);
    }

    noteOn(midiNote = 69, velocity = 0.8) {
        if (this.noteActive) {
            return;
        }
        this.api.noteOn(this.session, midiNote, velocity);
        this.noteActive = true;
    }

    noteOff() {
        if (!this.noteActive) {
            return;
        }
        this.api.noteOff(this.session);
        this.noteActive = false;
    }
}
