import createWebxedModule from './webxed.js';

const startButton = document.getElementById('startButton');
const noteButton = document.getElementById('noteButton');

let audioContext;
let processor;
let module;
let synth;
let noteActive = false;

startButton.addEventListener('click', async () => {
    if (audioContext) {
        return;
    }

    audioContext = new AudioContext();
    module = await createWebxedModule();

    const createSynth = module.cwrap('createSynth', 'number', ['number']);
    const noteOn = module.cwrap('noteOn', null, ['number', 'number', 'number']);
    const noteOff = module.cwrap('noteOff', null, ['number']);
    const renderSample = module.cwrap('renderSample', 'number', ['number']);

    synth = createSynth(audioContext.sampleRate);
    processor = audioContext.createScriptProcessor(512, 0, 1);
    processor.onaudioprocess = event => {
        const output = event.outputBuffer.getChannelData(0);
        for (let index = 0; index < output.length; index += 1) {
            output[index] = renderSample(synth);
        }
    };
    processor.connect(audioContext.destination);

    noteButton.addEventListener('pointerdown', () => {
        noteOn(synth, 69, 0.8);
        noteActive = true;
    });

    const stopNote = () => {
        if (noteActive) {
            noteOff(synth);
            noteActive = false;
        }
    };

    noteButton.addEventListener('pointerup', stopNote);
    noteButton.addEventListener('pointerleave', stopNote);

    noteButton.disabled = false;
    startButton.disabled = true;
});
