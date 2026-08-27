import createWebxedModule from './webxed.js';
import { AudioEngine } from './AudioEngine.js';
import { PatchBrowser } from './PatchBrowser.js';
import { SysexLoader } from './SysexLoader.js';
import { WebxedApi } from './WebxedApi.js';

const startButton = document.getElementById('startButton');
const sysexInput = document.getElementById('sysexInput');
const patchSelect = document.getElementById('patchSelect');
const previousButton = document.getElementById('previousButton');
const nextButton = document.getElementById('nextButton');
const dxNoteButton = document.getElementById('dxNoteButton');
const digitoneNoteButton = document.getElementById('digitoneNoteButton');
const status = document.getElementById('status');

let audioEngine;
let patchBrowser;
let sysexLoader;

startButton.addEventListener('click', async () => {
    if (audioEngine) {
        return;
    }

    const audioContext = new AudioContext();
    const module = await createWebxedModule();
    const api = new WebxedApi(module);
    const session = api.createSession(audioContext.sampleRate);

    audioEngine = new AudioEngine(api, session, audioContext);
    patchBrowser = new PatchBrowser(api, session, patchSelect, previousButton, nextButton, status);
    sysexLoader = new SysexLoader(api, session, patchBrowser, status);

    sysexInput.disabled = false;
    dxNoteButton.disabled = false;
    digitoneNoteButton.disabled = false;
    startButton.disabled = true;
    status.textContent = 'Audio ready. Load a .syx file or audition the DX/Digitone init voices.';
    patchBrowser.refreshNavigation();
});

sysexInput.addEventListener('change', async event => {
    const [file] = event.target.files;
    if (file && sysexLoader) {
        await sysexLoader.load(file);
    }
});

patchSelect.addEventListener('change', () => patchBrowser?.select(patchSelect.selectedIndex));
previousButton.addEventListener('click', () => patchBrowser?.previous());
nextButton.addEventListener('click', () => patchBrowser?.next());

dxNoteButton.addEventListener('pointerdown', () => audioEngine?.noteOn());
dxNoteButton.addEventListener('pointerup', () => audioEngine?.noteOff());
dxNoteButton.addEventListener('pointerleave', () => audioEngine?.noteOff());

digitoneNoteButton.addEventListener('pointerdown', () => {
    if (audioEngine?.selectPreviewEngine(1)) {
        audioEngine.noteOn();
    }
});
digitoneNoteButton.addEventListener('pointerup', () => audioEngine?.noteOff());
digitoneNoteButton.addEventListener('pointerleave', () => audioEngine?.noteOff());

window.addEventListener('keydown', event => {
    if (event.target === patchSelect || event.target === sysexInput) {
        return;
    }

    if (event.key === 'ArrowLeft') {
        patchBrowser?.previous();
    } else if (event.key === 'ArrowRight') {
        patchBrowser?.next();
    } else if (event.code === 'Space' && !event.repeat) {
        event.preventDefault();
        audioEngine?.selectPreviewEngine(0);
        audioEngine?.noteOn();
    } else if (event.key.toLowerCase() === 'n' && !event.repeat) {
        audioEngine?.selectPreviewEngine(1);
        audioEngine?.noteOn();
    } else if (event.key.toLowerCase() === 'd' && !event.repeat) {
        audioEngine?.selectPreviewEngine(0);
        audioEngine?.noteOn();
    }
});

window.addEventListener('keyup', event => {
    if (event.code === 'Space' || event.key.toLowerCase() === 'n' || event.key.toLowerCase() === 'd') {
        audioEngine?.noteOff();
    }
});
