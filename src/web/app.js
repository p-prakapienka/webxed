import createWebxedModule from './webxed.js';

const startButton = document.getElementById('startButton');
const sysexInput = document.getElementById('sysexInput');
const patchSelect = document.getElementById('patchSelect');
const previousButton = document.getElementById('previousButton');
const nextButton = document.getElementById('nextButton');
const noteButton = document.getElementById('noteButton');
const status = document.getElementById('status');

let audioContext;
let processor;
let module;
let synth;
let api;
let noteActive = false;

function refreshNavigation() {
    const count = patchSelect.options.length;
    const enabled = count > 0;
    patchSelect.disabled = !enabled;
    previousButton.disabled = !enabled || patchSelect.selectedIndex <= 0;
    nextButton.disabled = !enabled || patchSelect.selectedIndex >= count - 1;
}

function selectPatch(index) {
    if (!api || index < 0 || index >= patchSelect.options.length) {
        return;
    }
    if (api.selectPatch(synth, index)) {
        patchSelect.selectedIndex = index;
        refreshNavigation();
        status.textContent = `Selected ${index + 1}/${patchSelect.options.length}: ${patchSelect.options[index].text}`;
    }
}

async function loadSysex(file) {
    const bytes = new Uint8Array(await file.arrayBuffer());
    const pointer = module._malloc(bytes.length);
    try {
        module.HEAPU8.set(bytes, pointer);
        const count = api.loadSysex(synth, pointer, bytes.length);
        if (count < 1) {
            status.textContent = 'Could not parse this DX7 SysEx file.';
            return;
        }

        patchSelect.replaceChildren();
        for (let index = 0; index < count; index += 1) {
            const option = document.createElement('option');
            option.value = String(index);
            option.textContent = api.patchName(synth, index) || `Patch ${index + 1}`;
            patchSelect.append(option);
        }

        selectPatch(0);
        status.textContent = `Loaded ${file.name}: ${count} patch${count === 1 ? '' : 'es'}.`;
    } finally {
        module._free(pointer);
    }
}

startButton.addEventListener('click', async () => {
    if (audioContext) {
        return;
    }

    audioContext = new AudioContext();
    module = await createWebxedModule();
    api = {
        createSynth: module.cwrap('createSynth', 'number', ['number']),
        loadSysex: module.cwrap('loadSysex', 'number', ['number', 'number', 'number']),
        patchName: module.cwrap('patchName', 'string', ['number', 'number']),
        selectPatch: module.cwrap('selectPatch', 'number', ['number', 'number']),
        noteOn: module.cwrap('noteOn', null, ['number', 'number', 'number']),
        noteOff: module.cwrap('noteOff', null, ['number']),
        renderSample: module.cwrap('renderSample', 'number', ['number'])
    };

    synth = api.createSynth(audioContext.sampleRate);
    processor = audioContext.createScriptProcessor(512, 0, 1);
    processor.onaudioprocess = event => {
        const output = event.outputBuffer.getChannelData(0);
        for (let index = 0; index < output.length; index += 1) {
            output[index] = api.renderSample(synth);
        }
    };
    processor.connect(audioContext.destination);

    sysexInput.disabled = false;
    patchSelect.disabled = false;
    noteButton.disabled = false;
    startButton.disabled = true;
    status.textContent = 'Audio ready. Load a .syx file or audition the init voice.';
    refreshNavigation();
});

sysexInput.addEventListener('change', async event => {
    const [file] = event.target.files;
    if (file) {
        await loadSysex(file);
    }
});

patchSelect.addEventListener('change', () => selectPatch(patchSelect.selectedIndex));
previousButton.addEventListener('click', () => selectPatch(patchSelect.selectedIndex - 1));
nextButton.addEventListener('click', () => selectPatch(patchSelect.selectedIndex + 1));

noteButton.addEventListener('pointerdown', () => {
    if (!api || noteActive) {
        return;
    }
    api.noteOn(synth, 69, 0.8);
    noteActive = true;
});

function stopNote() {
    if (api && noteActive) {
        api.noteOff(synth);
        noteActive = false;
    }
}

noteButton.addEventListener('pointerup', stopNote);
noteButton.addEventListener('pointerleave', stopNote);

window.addEventListener('keydown', event => {
    if (event.target === patchSelect || event.target === sysexInput) {
        return;
    }
    if (event.key === 'ArrowLeft') {
        selectPatch(patchSelect.selectedIndex - 1);
    } else if (event.key === 'ArrowRight') {
        selectPatch(patchSelect.selectedIndex + 1);
    } else if (event.code === 'Space' && !event.repeat && api) {
        event.preventDefault();
        api.noteOn(synth, 69, 0.8);
        noteActive = true;
    }
});

window.addEventListener('keyup', event => {
    if (event.code === 'Space') {
        stopNote();
    }
});
