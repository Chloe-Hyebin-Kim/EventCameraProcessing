
#ifndef ECF_CODEC_EXPORT_H
#define ECF_CODEC_EXPORT_H

#ifdef ECF_CODEC_STATIC_DEFINE
#  define ECF_CODEC_EXPORT
#  define ECF_CODEC_NO_EXPORT
#else
#  ifndef ECF_CODEC_EXPORT
#    ifdef hdf5_ecf_codec_EXPORTS
        /* We are building this library */
#      define ECF_CODEC_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define ECF_CODEC_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef ECF_CODEC_NO_EXPORT
#    define ECF_CODEC_NO_EXPORT 
#  endif
#endif

#ifndef ECF_CODEC_DEPRECATED
#  define ECF_CODEC_DEPRECATED __declspec(deprecated)
#endif

#ifndef ECF_CODEC_DEPRECATED_EXPORT
#  define ECF_CODEC_DEPRECATED_EXPORT ECF_CODEC_EXPORT ECF_CODEC_DEPRECATED
#endif

#ifndef ECF_CODEC_DEPRECATED_NO_EXPORT
#  define ECF_CODEC_DEPRECATED_NO_EXPORT ECF_CODEC_NO_EXPORT ECF_CODEC_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef ECF_CODEC_NO_DEPRECATED
#    define ECF_CODEC_NO_DEPRECATED
#  endif
#endif

#endif /* ECF_CODEC_EXPORT_H */
