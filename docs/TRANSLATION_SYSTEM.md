# Translation System

HelixScreen's translation system combines LVGL's native XML translations for static UI text with an lv_i18n-compatible C API for runtime lookups and plural forms. This document explains how translations flow from source YAML files through code generation to the running application.

## Supported Languages

| Code | Language   |
|------|------------|
| de   | German     |
| en   | English    |
| es   | Spanish    |
| fr   | French     |
| it   | Italian    |
| ja   | Japanese   |
| pt   | Portuguese |
| ru   | Russian    |
| zh   | Chinese    |

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           translations/*.yml                            │
│                    (Master source files - one per locale)               │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
                                 ▼
                   ┌─────────────────────────────┐
                   │  generate_translations.py   │
                   │  (Python code generator)    │
                   └─────────────┬───────────────┘
                                 │
                ┌────────────────┼────────────────┐
                ▼                ▼                ▼
┌───────────────────┐ ┌──────────────────┐ ┌─────────────────────┐
│  translations.xml │ │ lv_i18n_trans... │ │ lv_i18n_trans...    │
│  (LVGL XML)       │ │ lations.c        │ │ lations.h           │
└─────────┬─────────┘ └────────┬─────────┘ └──────────┬──────────┘
          │                    │                      │
          ▼                    ▼                      ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          Runtime (Application)                          │
│  • XML parser reads translations.xml for static labels                  │
│  • C code provides lv_i18n_*() API for locale switching                 │
│  • Plural rules applied per-language                                    │
└─────────────────────────────────────────────────────────────────────────┘
```

## Source Files (YAML)

Translation source files live in `translations/` with one file per locale.

### File Format

```yaml
locale: en
translations:
  # Simple singular strings
  "Cancel": "Cancel"
  "Save": "Save"
  "Temperature": "Temperature"

  # Strings with special characters need quoting
  "Nozzle: {temp}°C": "Nozzle: {temp}°C"

  # Plural forms (dictionary)
  "items_selected":
    one: "{count} item selected"
    other: "{count} items selected"
```

### Key Rules

- **Keys are English text** - The translation key is the English version of the string
- **Singular translations** - Direct string-to-string mapping
- **Plural translations** - Dictionary with CLDR plural categories

### Example: Russian with Complex Plurals

Russian uses one/few/many/other categories:

```yaml
locale: ru
translations:
  "items_selected":
    one: "{count} элемент выбран"       # 1, 21, 31...
    few: "{count} элемента выбрано"     # 2-4, 22-24...
    many: "{count} элементов выбрано"   # 5-20, 25-30...
    other: "{count} элемента выбрано"   # Decimals
```

## Build Pipeline

### Generator Script

`scripts/generate_translations.py` is the main code generator. It reads all YAML files and produces:

| Output | Path | Purpose |
|--------|------|---------|
| `translations.xml` | `ui_xml/translations/translations.xml` | LVGL XML format for declarative UI |
| `lv_i18n_translations.c` | `src/generated/lv_i18n_translations.c` | Singular/plural arrays, plural functions, language pack |
| `lv_i18n_translations.h` | `src/generated/lv_i18n_translations.h` | Types, structs, function declarations |

### Makefile Integration

Translation generation is integrated into the build via `mk/translations.mk`:

```makefile
# Regenerate if YAML or script changes
$(TRANS_GEN_C) $(TRANS_GEN_H) $(TRANS_XML): $(TRANS_YAML) $(TRANS_SCRIPT)
    python generate_translations.py
```

Run manually with:

```bash
make translations
```

## Generated Files

### translations.xml

LVGL's native XML translation format. Each `<translation>` element maps a tag to translations in each language:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<translations languages="de en es fr it ja pt ru zh">
  <translation tag="Cancel" de="Abbrechen" en="Cancel" es="Cancelar" fr="Annuler" .../>
  <translation tag="Save" de="Speichern" en="Save" es="Guardar" fr="Enregistrer" .../>
  <translation tag="Temperature" de="Temperatur" en="Temperature" es="Temperatura" .../>
</translations>
```

### lv_i18n_translations.h

Defines types and declares the public API:

```c
typedef enum {
    LV_I18N_PLURAL_TYPE_ZERO,
    LV_I18N_PLURAL_TYPE_ONE,
    LV_I18N_PLURAL_TYPE_TWO,
    LV_I18N_PLURAL_TYPE_FEW,
    LV_I18N_PLURAL_TYPE_MANY,
    LV_I18N_PLURAL_TYPE_OTHER,
    _LV_I18N_PLURAL_TYPE_NUM,
} lv_i18n_plural_type_t;

typedef struct {
    const char * locale_name;
    const char * * singulars;
    const char * * plurals[_LV_I18N_PLURAL_TYPE_NUM];
    uint8_t (*locale_plural_fn)(int32_t num);
} lv_i18n_lang_t;

extern const lv_i18n_language_pack_t lv_i18n_language_pack[];

int lv_i18n_init(const lv_i18n_language_pack_t * langs);
int lv_i18n_set_locale(const char * l_name);
const char * lv_i18n_get_current_locale(void);
```

### lv_i18n_translations.c

Contains all translation data and runtime functions:

- **Singular arrays** per locale (indexed by key)
- **Plural arrays** per locale and form (one, few, many, other)
- **Plural functions** implementing CLDR rules per language
- **Language pack** array linking everything together
- **Runtime API** implementation

## XML Usage

### Static Text with translation_tag

For labels whose text is known at compile time, use the `translation_tag` attribute:

```xml
<!-- Text translates based on current locale -->
<text_heading text="Temperature" translation_tag="Temperature"/>

<!-- Button with translated label -->
<ui_button text="Cancel" translation_tag="Cancel" variant="secondary"/>

<!-- Help text -->
<text_small text="Select 'None' if you don't have a sensor."
            translation_tag="Select 'None' if you don't have a sensor."/>
```

The `text` attribute serves as a fallback if the translation tag isn't found.

### Semantic Text Widgets

Use semantic text widgets (`text_heading`, `text_body`, `text_small`, `text_muted`) rather than raw `lv_label`:

```xml
<!-- ✅ Correct -->
<text_body text="Material" translation_tag="Material"/>

<!-- ❌ Avoid -->
<lv_label text="Material" style_text_font="..." translation_tag="Material"/>
```

### Dynamic Text with bind_text

For text that changes at runtime based on data, use `bind_text` with subjects instead of `translation_tag`:

```xml
<!-- Subject-bound text (NOT translated) -->
<text_body bind_text="printer_status_subject"/>

<!-- Temperature display from subject -->
<text_heading bind_text="nozzle_temp_display"/>
```

**Important:** `bind_text` and `translation_tag` serve different purposes:
- `translation_tag` - Static text that varies by locale
- `bind_text` - Dynamic text that varies by application state

## C++ Runtime API

### Initialization

The translation system is initialized during application startup:

```cpp
// In application.cpp
#include "lv_i18n_translations.h"

void Application::init_translations() {
    // Initialize with the language pack
    int result = lv_i18n_init(lv_i18n_language_pack);
    if (result != 0) {
        spdlog::error("Failed to initialize i18n");
        return;
    }

    // Set locale from settings (or default to "en")
    std::string lang = settings_manager.get_language();
    lv_i18n_set_locale(lang.c_str());
}
```

### Changing Language

To change the display language at runtime:

```cpp
// In settings_manager.cpp
void SettingsManager::set_language(const std::string& lang) {
    int result = lv_i18n_set_locale(lang.c_str());
    if (result == 0) {
        // Success - trigger UI refresh
        lv_obj_invalidate(lv_screen_active());
    }
}
```

### Querying Current Locale

```cpp
const char* current = lv_i18n_get_current_locale();
spdlog::info("Current language: {}", current);
```

## Plural Rules

Plural forms follow the [CLDR standard](https://unicode-org.github.io/cldr-staging/charts/latest/supplemental/language_plural_rules.html).

### Categories

| Category | Typical Usage |
|----------|---------------|
| `zero`   | Zero items (some languages only) |
| `one`    | Singular (1 item, or special cases) |
| `two`    | Dual (exactly 2, some languages) |
| `few`    | Paucal (2-4 in Slavic languages) |
| `many`   | Large numbers (5+ in Slavic languages) |
| `other`  | General plural / fallback |

### Language Examples

**English** (simple - one/other):
- 1 → one ("1 item")
- 0, 2, 3... → other ("0 items", "2 items")

**French** (0/1 singular):
- 0, 1 → one ("0 élément", "1 élément")
- 2, 3... → other ("2 éléments")

**Russian** (complex - one/few/many/other):
- 1, 21, 31... → one ("1 элемент")
- 2-4, 22-24... → few ("2 элемента")
- 5-20, 25-30... → many ("5 элементов")
- Decimals → other

### Generated Plural Functions

Each language gets a plural function implementing its rules:

```c
// English: simple one/other
static uint8_t en_plural_fn(int32_t num) {
    uint32_t i = op_i(op_n(num));
    uint32_t v = op_v(op_n(num));

    if ((i == 1 && v == 0)) return LV_I18N_PLURAL_TYPE_ONE;
    return LV_I18N_PLURAL_TYPE_OTHER;
}

// Russian: one/few/many/other
static uint8_t ru_plural_fn(int32_t num) {
    uint32_t i = op_i(op_n(num));
    uint32_t i10 = i % 10;
    uint32_t i100 = i % 100;

    if ((v == 0 && i10 == 1 && i100 != 11))
        return LV_I18N_PLURAL_TYPE_ONE;
    if ((v == 0 && (2 <= i10 && i10 <= 4) && (!(12 <= i100 && i100 <= 14))))
        return LV_I18N_PLURAL_TYPE_FEW;
    if ((v == 0 && i10 == 0) || (v == 0 && (5 <= i10 && i10 <= 9)) ||
        (v == 0 && (11 <= i100 && i100 <= 14)))
        return LV_I18N_PLURAL_TYPE_MANY;
    return LV_I18N_PLURAL_TYPE_OTHER;
}
```

## Developer Workflow

### Adding New Translatable Strings

1. **Add to XML with translation_tag:**
   ```xml
   <text_body text="New Feature" translation_tag="New Feature"/>
   ```

2. **Extract to YAML files:**
   ```bash
   make translation-sync
   ```
   This scans XML files and adds new keys to all YAML files with the English value as placeholder.

3. **Translate in YAML files:**
   Edit each `translations/*.yml` file to add proper translations:
   ```yaml
   # translations/de.yml
   "New Feature": "Neue Funktion"

   # translations/fr.yml
   "New Feature": "Nouvelle fonctionnalité"
   ```

4. **Regenerate code:**
   ```bash
   make translations
   ```

5. **Rebuild and test:**
   ```bash
   make -j && ./build/bin/helix-screen --test -vv
   ```

### Preview Changes Before Sync

```bash
make translation-sync-dry-run
```

This shows what keys would be added without modifying files.

## Makefile Targets

| Target | Description |
|--------|-------------|
| `make translations` | Regenerate C code and XML from YAML |
| `make translation-sync` | Extract strings from XML, merge new keys to YAML |
| `make translation-sync-dry-run` | Preview sync without modifying files |
| `make translation-coverage` | Show translation coverage statistics |
| `make translation-obsolete` | Find translation keys not used in XML |

## Key Files Reference

| File | Purpose |
|------|---------|
| `translations/*.yml` | Source translation files (one per language) |
| `scripts/generate_translations.py` | Main code generator |
| `scripts/translation_sync.py` | String extraction and sync tool |
| `scripts/translations/` | Support modules for sync tool |
| `mk/translations.mk` | Makefile integration |
| `ui_xml/translations/translations.xml` | Generated LVGL XML translations |
| `src/generated/lv_i18n_translations.c` | Generated C source |
| `src/generated/lv_i18n_translations.h` | Generated C header |
| `src/application/application.cpp` | Translation initialization |
| `src/system/settings_manager.cpp` | Locale switching |

## Troubleshooting

### Translation not appearing

1. Check the key exists in `translations/en.yml`
2. Verify `translation_tag` matches the key exactly
3. Run `make translations` to regenerate
4. Rebuild the application

### Missing translation warning

The generator warns about keys missing in non-English locales:
```
WARNING: Missing 'New Feature' in de
WARNING: Missing 'New Feature' in fr
```

Fill in missing translations in the appropriate YAML files.

### Plural form not working

1. Verify the key uses dictionary format in YAML
2. Check the language has a plural rule in `generate_translations.py`
3. Ensure you're using the correct plural categories for that language
