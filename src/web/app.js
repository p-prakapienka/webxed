const startButton = document.getElementById('startButton');
const sysexInput = document.getElementById('sysexInput');
const patchSelect = document.getElementById('patchSelect');
const previousButton = document.getElementById('previousButton');
const nextButton = document.getElementById('nextButton');
const convertButton = document.getElementById('convertButton');
const saveButton = document.getElementById('saveButton');
const dxNoteButton = document.getElementById('dxNoteButton');
const digitoneNoteButton = document.getElementById('digitoneNoteButton');
const status = document.getElementById('status');

let audioEngine;
let patchBrowser;
let sysexLoader;
let conversionPanel;

startButton.addEventListener('click', async () => {
    if (audioEngine) {
        return;
    }

    const audioContext = new AudioContext();
    const module = await createWebxedModule();
    const api = new WebxedApi(module);
    const session = api.createSession(audioContext.sampleRate);

    audioEngine = new AudioEngine(api, session, audioContext);
    conversionPanel = new ConversionPanel(api, session, {
        sourceName: document.getElementById('sourceName'),
        sourceAlgorithm: document.getElementById('sourceAlgorithm'),
        targetName: document.getElementById('targetName'),
        targetAlgorithm: document.getElementById('targetAlgorithm'),
        warnings: document.getElementById('warnings'),
        digitoneNoteButton,
        saveButton,
        status
    });
    patchBrowser = new PatchBrowser(
        api,
        session,
        patchSelect,
        previousButton,
        nextButton,
        status,
        () => conversionPanel.refresh()
    );
    sysexLoader = new SysexLoader(api, session, patchBrowser, status);

    sysexInput.disabled = false;
    dxNoteButton.disabled = false;
    convertButton.disabled = false;
    startButton.disabled = true;
    status.textContent = 'Audio ready. Load a .syx file or convert the DX init voice.';
    patchBrowser.refreshNavigation();
    conversionPanel.refresh();
});

sysexInput.addEventListener('change', async event => {
    const [file] = event.target.files;
    if (file && sysexLoader) {
        await sysexLoader.load(file);
        conversionPanel?.refresh();
    }
});

patchSelect.addEventListener('change', () => patchBrowser?.select(patchSelect.selectedIndex));
previousButton.addEventListener('click', () => patchBrowser?.previous());
nextButton.addEventListener('click', () => patchBrowser?.next());
convertButton.addEventListener('click', () => conversionPanel?.convert());
saveButton.addEventListener('click', () => conversionPanel?.save());

dxNoteButton.addEventListener('pointerdown', () => {
    if (audioEngine?.selectPreviewEngine(0)) {
        audioEngine.noteOn();
    }
});
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

    const key = event.key.toLowerCase();
    if (event.key === 'ArrowLeft') {
        patchBrowser?.previous();
    } else if (event.key === 'ArrowRight') {
        patchBrowser?.next();
    } else if (event.code === 'Space' && !event.repeat) {
        event.preventDefault();
        audioEngine?.selectPreviewEngine(0);
        audioEngine?.noteOn();
    } else if (key === 'n' && !event.repeat) {
        if (audioEngine?.selectPreviewEngine(1)) {
            audioEngine.noteOn();
        }
    } else if (key === 'd' && !event.repeat) {
        if (audioEngine?.selectPreviewEngine(0)) {
            audioEngine.noteOn();
        }
    } else if (key === 'c' && !event.repeat) {
        conversionPanel?.convert();
    } else if (key === 's' && !event.repeat) {
        event.preventDefault();
        conversionPanel?.save();
    }
});

window.addEventListener('keyup', event => {
    const key = event.key.toLowerCase();
    if (event.code === 'Space' || key === 'n' || key === 'd') {
        audioEngine?.noteOff();
    }
});
