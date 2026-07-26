/*
 * simplefile_filex.h — internal glue for the SimpleFile/SimpleDir API on FileX.
 *
 * The public API is the reference's simplefile.h / simpledir.h (unchanged, so
 * ported menu/drive code compiles against it verbatim). This header exposes
 * only what the port glue and tests need: media binding + the generation
 * counter used to invalidate stale handles across a card swap.
 */
#ifndef TONNEREXL_SIMPLEFILE_FILEX_H
#define TONNEREXL_SIMPLEFILE_FILEX_H

#include "fx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bind the adapter to an opened FX_MEDIA. Called by the SD lifecycle thread
 * after fx_media_open succeeds. Bumps the generation counter so any SimpleFile
 * opened against a previous card is treated as stale on next use.
 */
void simplefile_bind_media(FX_MEDIA *media);

/*
 * Unbind on card removal / media close. Bumps generation; subsequent operations
 * on previously-open SimpleFiles fail cleanly instead of touching freed media.
 */
void simplefile_unbind_media(void);

/* Current generation (test/introspection). */
unsigned simplefile_generation(void);

/*
 * The filesystem+lifecycle mutex. Created with priority inheritance. Held across
 * FileX operations so the SD lifecycle thread and any filesystem caller can't
 * race a media open/close against a file op. On host builds this is a real
 * ThreadX mutex too (linux port), so the locking path is under test.
 */
void simplefile_lock(void);
void simplefile_unlock(void);
void simplefile_init_lock(void);

#ifdef __cplusplus
}
#endif
#endif /* TONNEREXL_SIMPLEFILE_FILEX_H */
