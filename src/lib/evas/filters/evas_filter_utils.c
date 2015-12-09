/* Utility functions for the filters.  */

#include "evas_filter_private.h"

Evas_Filter_Buffer *
evas_filter_buffer_scaled_get(Evas_Filter_Context *ctx,
                              Evas_Filter_Buffer *src,
                              unsigned w, unsigned h)
{
   Evas_Filter_Buffer *fb;
   RGBA_Image *dstim, *srcim;
   RGBA_Draw_Context dc;
   Eina_Bool ok;

   // only for RGBA
   EINA_SAFETY_ON_FALSE_RETURN_VAL(!src->alpha_only, NULL);

   srcim = evas_filter_buffer_backing_get(ctx, src->id);
   EINA_SAFETY_ON_NULL_RETURN_VAL(srcim, NULL);

   fb = evas_filter_temporary_buffer_get(ctx, w, h, src->alpha_only);
   EINA_SAFETY_ON_NULL_RETURN_VAL(fb, NULL);

   dstim = evas_filter_buffer_backing_get(ctx, fb->id);
   EINA_SAFETY_ON_NULL_RETURN_VAL(dstim, NULL);
   EINA_SAFETY_ON_FALSE_RETURN_VAL((dstim->cache_entry.w == w) &&
                                   (dstim->cache_entry.h == h), NULL);

   memset(&dc, 0, sizeof(dc));
   dc.sli.h = 1;
   dc.render_op = EVAS_RENDER_COPY;

   ok = evas_common_scale_rgba_in_to_out_clip_smooth
         (srcim, dstim, &dc, 0, 0, src->w, src->h, 0, 0, w, h);

   if (!ok)
     {
       ERR("RGBA Image scaling failed.");
       return NULL;
     }

   return fb;
}

static Eina_Bool
_interpolate_none(DATA8 *output, int *points)
{
   DATA8 val = 0;
   int j;
   for (j = 0; j < 256; j++)
     {
        if (points[j] == -1)
          output[j] = val;
        else
          val = output[j] = (DATA8) points[j];
     }
   return EINA_TRUE;
}

static Eina_Bool
_interpolate_linear(DATA8 *output, int *points)
{
   DATA8 val = 0;
   int j, k, last_idx = 0;
   for (j = 0; j < 256; j++)
     {
        if (points[j] != -1)
          {
             output[j] = (DATA8) points[j];
             for (k = last_idx + 1; k < j; k++)
               output[k] = (DATA8) (points[j] + ((k - last_idx) * (points[j] - points[last_idx]) / (j - last_idx)));
             last_idx = j;
          }
     }
   val = (DATA8) points[last_idx];
   for (j = last_idx + 1; j < 256; j++)
     output[j] = val;
   return EINA_TRUE;
}

Eina_Bool
evas_filter_interpolate(DATA8 *output, int *points,
                        Evas_Filter_Interpolation_Mode mode)
{
   switch (mode)
     {
      case EVAS_FILTER_INTERPOLATION_MODE_NONE:
        return _interpolate_none(output, points);
      case EVAS_FILTER_INTERPOLATION_MODE_LINEAR:
      default:
        return _interpolate_linear(output, points);
     }
}

int
evas_filter_smallest_pow2_larger_than(int val)
{
   int n;

   for (n = 0; n < 32; n++)
     if (val <= (1 << n)) return n;

   ERR("Value %d is too damn high!", val);
   return 32;
}
