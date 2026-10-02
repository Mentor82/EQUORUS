// EQUORUS Studio - Main Application Controller

import { EquorusValidator } from './core/validator.js';
import { serializeCanonicalBytes } from './core/canonical.js';
import { computeEnvelopeIntegrity, verifyEnvelopeIntegrity } from './core/integrity.js';
import { PRESETS } from './fixtures/presets.js';
import { EditorView } from './ui/editor_view.js';
import { DiffView } from './ui/diff_view.js';
import { IntegrityView } from './ui/integrity_view.js';
import { AdapterView } from './ui/adapter_view.js';
import { CodegenView } from './ui/codegen_view.js';

export class EquorusStudioApp {
  constructor() {
    this.state = {
      rawJson: '',
      validationResult: null,
      canonicalBytes: null,
      integrityRecord: null,
      isTampered: false,
      activeRightTab: 'diff'
    };

    this.init();
  }

  init() {
    // 1. Setup Right Pane Tabs
    const tabButtons = document.querySelectorAll('.right-tab-btn');
    tabButtons.forEach(btn => {
      btn.addEventListener('click', (e) => {
        const tab = btn.dataset.tab;
        this._switchRightTab(tab);
      });
    });

    // 2. Initialize Views
    this.editorView = new EditorView({
      container: document.getElementById('editor-container'),
      onInputChange: (val) => this.handleInputChange(val),
      onPresetChange: (presetId) => this.handlePresetChange(presetId)
    });
    this.editorView.setPresets(PRESETS);

    this.diffView = new DiffView({
      container: document.getElementById('diff-container')
    });

    this.integrityView = new IntegrityView({
      container: document.getElementById('integrity-container'),
      onTamperToggle: (isTampered) => this.handleTamperToggle(isTampered)
    });

    this.adapterView = new AdapterView({
      container: document.getElementById('adapter-container')
    });

    this.codegenView = new CodegenView({
      container: document.getElementById('codegen-container')
    });

    // 3. Load default initial preset
    this.handlePresetChange('vinox-full');
  }

  _switchRightTab(tabId) {
    this.state.activeRightTab = tabId;
    const tabButtons = document.querySelectorAll('.right-tab-btn');
    tabButtons.forEach(btn => {
      if (btn.dataset.tab === tabId) {
        btn.className = 'right-tab-btn text-xs font-bold px-3.5 py-1.5 rounded-lg bg-[var(--primary)] text-white shadow-xs transition-all cursor-pointer';
      } else {
        btn.className = 'right-tab-btn text-xs font-medium px-3.5 py-1.5 rounded-lg text-[var(--muted-foreground)] hover:text-[var(--foreground)] hover:bg-[var(--card)] transition-all cursor-pointer';
      }
    });

    const panels = {
      diff: document.getElementById('diff-container'),
      integrity: document.getElementById('integrity-container'),
      adapter: document.getElementById('adapter-container'),
      codegen: document.getElementById('codegen-container')
    };

    for (const [key, el] of Object.entries(panels)) {
      if (el) {
        el.style.display = (key === tabId) ? 'block' : 'none';
      }
    }
  }

  handlePresetChange(presetId) {
    const preset = PRESETS.find(p => p.id === presetId);
    if (!preset) return;
    this.state.isTampered = false;
    this.editorView.setValue(preset.json);
    this.handleInputChange(preset.json);
  }

  async handleInputChange(rawText) {
    this.state.rawJson = rawText;
    this.state.isTampered = false;

    // Validate
    const valResult = EquorusValidator.validate(rawText);
    this.state.validationResult = valResult;
    this.editorView.setValidationStatus(valResult);

    if (!valResult.valid || !valResult.parsedObject) {
      this.state.canonicalBytes = null;
      this.state.integrityRecord = null;
      this.diffView.update({ canonicalBytes: null, rawText });
      this.integrityView.update({ record: null, canonicalBytes: null, isValid: false, isTampered: false });
      this.adapterView.update(null);
      this.codegenView.update({ rawJson: null, typeId: null, isValid: false });
      return;
    }

    // Process canonical bytes & integrity
    try {
      const envelopeObj = valResult.parsedObject;
      const canonicalBytes = serializeCanonicalBytes(envelopeObj);
      this.state.canonicalBytes = canonicalBytes;

      const integrity = await computeEnvelopeIntegrity(envelopeObj);
      this.state.integrityRecord = integrity.record;

      // Update all view components
      this.diffView.update({ canonicalBytes, rawText });
      this.integrityView.update({
        record: integrity.record,
        canonicalBytes,
        isValid: true,
        isTampered: false
      });
      this.adapterView.update(envelopeObj);
      this.codegenView.update({
        rawJson: rawText,
        typeId: valResult.typeId,
        isValid: true
      });
    } catch (e) {
      console.error('Processing error:', e);
    }
  }

  async handleTamperToggle(isTampered) {
    this.state.isTampered = isTampered;
    if (!this.state.validationResult || !this.state.validationResult.valid || !this.state.canonicalBytes) {
      return;
    }

    if (isTampered) {
      // Simulate bit-flip: mutate the first byte of payload
      const tamperedBytes = new Uint8Array(this.state.canonicalBytes);
      if (tamperedBytes.length > 5) {
        tamperedBytes[tamperedBytes.length - 2] ^= 0x01; // flip 1 bit near the end
      }
      this.diffView.update({ canonicalBytes: tamperedBytes, rawText: this.state.rawJson });
      this.integrityView.update({
        record: this.state.integrityRecord,
        canonicalBytes: tamperedBytes,
        isValid: true,
        isTampered: true
      });
    } else {
      this.diffView.update({ canonicalBytes: this.state.canonicalBytes, rawText: this.state.rawJson });
      this.integrityView.update({
        record: this.state.integrityRecord,
        canonicalBytes: this.state.canonicalBytes,
        isValid: true,
        isTampered: false
      });
    }
  }
}

// Auto-boot on DOM ready
document.addEventListener('DOMContentLoaded', () => {
  window.equorusStudio = new EquorusStudioApp();
});
