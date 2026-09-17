class DigitoneEditor {
    constructor(api, session, conversionPanel, elements) {
        this.api = api;
        this.session = session;
        this.conversionPanel = conversionPanel;
        this.elements = elements;
        this.applying = false;
        this.bind();
        this.refresh();
    }

    bind() {
        this.elements.editButton.addEventListener('click', () => this.toggle());
        this.elements.form.addEventListener('submit', event => event.preventDefault());
        this.elements.form.addEventListener('input', () => this.apply());
        this.elements.form.addEventListener('change', () => this.apply());
    }

    toggle() {
        if (!this.converted()) {
            this.elements.status.textContent = 'Convert a patch before editing.';
            return false;
        }

        this.elements.panel.hidden = !this.elements.panel.hidden;
        this.elements.status.textContent = this.elements.panel.hidden
            ? 'Editor hidden.'
            : 'Editing converted Digitone patch.';
        return !this.elements.panel.hidden;
    }

    show() {
        if (!this.converted()) {
            this.elements.panel.hidden = true;
            this.elements.editButton.disabled = true;
            return false;
        }

        this.elements.panel.hidden = false;
        this.elements.editButton.disabled = false;
        return true;
    }

    converted() {
        return Boolean(this.conversionPanel.state().converted);
    }

    refresh() {
        const state = this.conversionPanel.state();
        this.elements.editButton.disabled = !state.converted;
        if (!state.converted || !state.patch) {
            this.elements.panel.hidden = true;
            return;
        }

        this.applying = true;
        const patch = state.patch;
        this.elements.name.value = patch.name;
        this.elements.algorithm.value = String(patch.algorithm);
        this.elements.ratioC.value = String(patch.ratios.c);
        this.elements.ratioA.value = String(patch.ratios.a);
        this.elements.ratioB1.value = String(patch.ratios.b1);
        this.elements.ratioB2.value = String(patch.ratios.b2);
        this.elements.harmonic.value = String(patch.harmonic);
        this.elements.detune.value = String(patch.detune);
        this.elements.feedback.value = String(patch.feedback);
        this.elements.mix.value = String(patch.mix);
        this.fillEnvelope(this.elements.envelopeA, patch.envelopes.a);
        this.fillEnvelope(this.elements.envelopeB, patch.envelopes.b);
        this.applying = false;
    }

    fillEnvelope(fields, envelope) {
        fields.attack.value = String(envelope.attack);
        fields.decay.value = String(envelope.decay);
        fields.endLevel.value = String(envelope.endLevel);
        fields.level.value = String(envelope.level);
        fields.delay.value = String(envelope.delay);
        fields.triggered.checked = envelope.triggered;
        fields.reset.checked = envelope.reset;
    }

    readEnvelope(fields) {
        return {
            attack: Number(fields.attack.value),
            decay: Number(fields.decay.value),
            endLevel: Number(fields.endLevel.value),
            level: Number(fields.level.value),
            delay: Number(fields.delay.value),
            triggered: fields.triggered.checked,
            reset: fields.reset.checked
        };
    }

    apply() {
        if (this.applying || !this.converted()) {
            return false;
        }

        const state = this.conversionPanel.state();
        const patch = {
            format: 'webxed-digitone-patch',
            version: 1,
            name: this.elements.name.value,
            algorithm: Number(this.elements.algorithm.value),
            ratios: {
                c: Number(this.elements.ratioC.value),
                a: Number(this.elements.ratioA.value),
                b1: Number(this.elements.ratioB1.value),
                b2: Number(this.elements.ratioB2.value)
            },
            harmonic: Number(this.elements.harmonic.value),
            detune: Number(this.elements.detune.value),
            feedback: Number(this.elements.feedback.value),
            mix: Number(this.elements.mix.value),
            envelopes: {
                a: this.readEnvelope(this.elements.envelopeA),
                b: this.readEnvelope(this.elements.envelopeB)
            }
        };

        if (!this.api.loadPatch(this.session, JSON.stringify(patch))) {
            this.elements.status.textContent = 'Edit rejected. Check parameter ranges.';
            this.refresh();
            return false;
        }

        this.conversionPanel.refresh();
        this.elements.status.textContent =
            `Updated ${patch.name || state.target?.name || 'patch'} (algorithm ${patch.algorithm}).`;
        return true;
    }
}
