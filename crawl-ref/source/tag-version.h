#ifndef TAG_VERSION_H
#define TAG_VERSION_H

// Character info has its own top-level tag, mismatching majors don't break
// compatibility there.
#define TAG_CHR_FORMAT 0

// Let CDO updaters know if the syntax changes.
#define TAG_MAJOR_VERSION 34

// Minor version will be reset to zero when major version changes.
enum tag_minor_version
{
    TAG_MINOR_INVALID         = -1,
    TAG_MINOR_RESET           = 0, // Minor tags were reset
    TAG_MINOR_BRANCHES_LEFT,       // Note the first time branches are left
    TAG_MINOR_VAULT_LIST,          // Don't try to store you.vault_list as prop
    TAG_MINOR_TRAPS_DETERM,        // Searching for traps is deterministic.
    TAG_MINOR_ACTION_THROW,        // Store base type of throw objects.
    TAG_MINOR_TEMP_MUTATIONS,      // Enable transient mutations
    TAG_MINOR_AUTOINSCRIPTIONS,    // Artefact inscriptions are added on the fly
    TAG_MINOR_UNCANCELLABLES,      // Restart uncancellable questions upon save load
    TAG_MINOR_DEEP_ABYSS,          // Multi-level abyss
    TAG_MINOR_COORD_SERIALIZER,    // Serialize coord_def as int
    TAG_MINOR_REMOVE_ABYSS_SEED,   // Remove the abyss seed.
    TAG_MINOR_REIFY_SUBVAULTS,     // Save subvaults with level for attribution
    TAG_MINOR_VEHUMET_SPELL_GIFT,  // Vehumet gift spells instead of books
    TAG_MINOR_0_11 = 17,           // 0.11 final saves
    TAG_MINOR_0_12,                // (no change)
    TAG_MINOR_BATTLESPHERE_MID,    // Monster battlesphere (mid of creator)
    TAG_MINOR_MALMUTATE,           // Convert Polymorph to Malmutate on old monsters
    TAG_MINOR_VEHUMET_MULTI_GIFTS, // Vehumet can offer multiple spells at once
    TAG_MINOR_ADD_ABYSS_SEED,      // Reinstate abyss seed. Mistakes were made.
    TAG_MINOR_COMPANION_LIST,      // Added companion list
    TAG_MINOR_INCREMENTAL_RECALL,  // Made recall incremental
    NUM_TAG_MINORS,
    TAG_MINOR_VERSION = NUM_TAG_MINORS - 1
};

#endif
