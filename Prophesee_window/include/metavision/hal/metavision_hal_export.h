
#ifndef METAVISION_HAL_EXPORT_H
#define METAVISION_HAL_EXPORT_H

#ifdef METAVISION_HAL_STATIC_DEFINE
#  define METAVISION_HAL_EXPORT
#  define METAVISION_HAL_NO_EXPORT
#else
#  ifndef METAVISION_HAL_EXPORT
#    ifdef metavision_hal_EXPORTS
        /* We are building this library */
#      define METAVISION_HAL_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define METAVISION_HAL_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef METAVISION_HAL_NO_EXPORT
#    define METAVISION_HAL_NO_EXPORT 
#  endif
#endif

#ifndef METAVISION_HAL_DEPRECATED
#  define METAVISION_HAL_DEPRECATED __declspec(deprecated)
#endif

#ifndef METAVISION_HAL_DEPRECATED_EXPORT
#  define METAVISION_HAL_DEPRECATED_EXPORT METAVISION_HAL_EXPORT METAVISION_HAL_DEPRECATED
#endif

#ifndef METAVISION_HAL_DEPRECATED_NO_EXPORT
#  define METAVISION_HAL_DEPRECATED_NO_EXPORT METAVISION_HAL_NO_EXPORT METAVISION_HAL_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef METAVISION_HAL_NO_DEPRECATED
#    define METAVISION_HAL_NO_DEPRECATED
#  endif
#endif

        #ifndef METAVISION_HAL_EXTERN_EXPORT 
        #  define METAVISION_HAL_EXTERN_EXPORT __declspec(dllexport)
        #endif
#endif /* METAVISION_HAL_EXPORT_H */
