#include "gl_engine_filter.h"

static Eina_Bool
_gl_filter_blur(Render_Engine_GL_Generic *re, Evas_Filter_Command *cmd)
{
   Evas_Engine_GL_Context *gc;
   Evas_GL_Image *image, *surface;
   RGBA_Draw_Context *dc_save;
   Eina_Bool horiz;
   double sx, sy, sw, sh, ssx, ssy, ssw, ssh, dx, dy, dw, dh, radius;
   int nx, ny, nw, nh;

   DEBUG_TIME_BEGIN();

   re->window_use(re->software.ob);
   gc = re->window_gl_context_get(re->software.ob);

   image = evas_ector_buffer_drawable_image_get(cmd->input->buffer);
   EINA_SAFETY_ON_NULL_RETURN_VAL(image, EINA_FALSE);

   surface = evas_ector_buffer_render_image_get(cmd->output->buffer);
   EINA_SAFETY_ON_NULL_RETURN_VAL(surface, EINA_FALSE);

   evas_gl_common_context_target_surface_set(gc, surface);

   dc_save = gc->dc;
   gc->dc = evas_common_draw_context_new();
   evas_common_draw_context_set_multiplier(gc->dc, cmd->draw.R, cmd->draw.G, cmd->draw.B, cmd->draw.A);

   if (cmd->input == cmd->output)
     gc->dc->render_op = EVAS_RENDER_COPY;
   else
     gc->dc->render_op = _gfx_to_evas_render_op(cmd->draw.rop);

   if (cmd->blur.dx)
     {
        horiz = EINA_TRUE;
        radius = cmd->blur.dx;
     }
   else
     {
        horiz = EINA_FALSE;
        radius = cmd->blur.dy;
     }

   DBG("blur %d @%p -> %d @%p (%.0fpx %s)",
       cmd->input->id, cmd->input->buffer, cmd->output->id, cmd->output->buffer,
       radius, horiz ? "X" : "Y");

   sx = 0;
   sy = 0;
   sw = cmd->input->w;
   sh = cmd->input->h;
   dx = cmd->draw.ox;
   dy = cmd->draw.oy;
   dw = cmd->output->w;
   dh = cmd->output->h;

   nx = dx; ny = dy; nw = dw; nh = dh;
   RECTS_CLIP_TO_RECT(nx, ny, nw, nh, 0, 0, cmd->output->w, cmd->output->h);
   ssx = (double)sx + ((double)(sw * (nx - dx)) / (double)(dw));
   ssy = (double)sy + ((double)(sh * (ny - dy)) / (double)(dh));
   ssw = ((double)sw * (double)(nw)) / (double)(dw);
   ssh = ((double)sh * (double)(nh)) / (double)(dh);

   evas_gl_common_filter_blur_push(gc, image->tex, ssx, ssy, ssw, ssh, dx, dy, dw, dh, radius, horiz);

   evas_common_draw_context_free(gc->dc);
   gc->dc = dc_save;

   evas_ector_buffer_engine_image_release(cmd->input->buffer, image);
   evas_ector_buffer_engine_image_release(cmd->output->buffer, surface);

   DEBUG_TIME_END();

   return EINA_TRUE;
}

GL_Filter_Apply_Func
gl_filter_blur_func_get(Evas_Filter_Command *cmd)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cmd, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cmd->output, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cmd->input, NULL);
   EINA_SAFETY_ON_FALSE_RETURN_VAL((!cmd->blur.dx) ^ (!cmd->blur.dy), NULL);

   return _gl_filter_blur;
}