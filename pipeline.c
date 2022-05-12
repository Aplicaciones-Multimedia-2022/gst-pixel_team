#include <gst/gst.h>
#include <glib.h>

// Mensajes para el bus
static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

// Gestión dinámica de pads para el demultiplexor
static void
on_pad_added (GstElement *element,
              GstPad     *pad,
              gpointer    data)
{
  GstPad *sinkpad;
  GstElement *decoder = (GstElement *) data;

  /* We can now link this pad with the vorbis-decoder sink pad */
  g_print ("Dynamic pad created, linking demuxer/decoder\n");

  sinkpad = gst_element_get_static_pad (decoder, "sink");

  gst_pad_link (pad, sinkpad);

  gst_object_unref (sinkpad);
}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;
  GstBus *bus;
  guint bus_watch_id;

  GstElement *pipeline, *source, *demuxer;
  GstElement *vdecoder, *adecoder;
  GstElement *vconv, *aconv;
  GstElement *effect;
  GstElement *vqueue, *aqueue;
  GstElement *asink, *vsink;

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  if (argc != 2) {
    g_printerr ("Usage: %s <Ogg filename>\n", argv[0]);
    return -1;
  }


  // Creo todos los elementos de la pipeline
  pipeline = gst_pipeline_new ("audio-player");
  source   = gst_element_factory_make ("filesrc",       "file-source");
  demuxer  = gst_element_factory_make ("oggdemux",      "ogg-demuxer");
  vdecoder = gst_element_factory_make ("theoradec",     "theora-decoder");
  vconv    = gst_element_factory_make ("videoconvert",  "vconverter");
  vsink    = gst_element_factory_make ("autovideosink", "video-output");
  adecoder = gst_element_factory_make ("vorbisdec",     "vorbis-decoder");
  aconv    = gst_element_factory_make ("audioconvert",  "aconverter");
  asink    = gst_element_factory_make ("autoaudiosink", "audio-output");
  vqueue   = gst_element_factory_make ("queue",         "queue−video");
  aqueue   = gst_element_factory_make ("queue",         "queue−audio");
  effect   = gst_element_factory_make ("videobalance",  "video-effect");

  // Compruebo que todos se hayan creado
  if (!pipeline || !source || !demuxer || !vdecoder || !adecoder || !vconv || !aconv 
      || !vqueue || !aqueue || !effect || !asink || !vsink || !effect) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  // Configuro los elementos 
  g_object_set (G_OBJECT (source), "location", argv[1], NULL);
  g_object_set (G_OBJECT (effect), "saturation", 0.0, NULL); // web server

  // Mensajes para el bus
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  // Se agrega todo a la pipeline
  gst_bin_add_many (GST_BIN (pipeline),
                    source, demuxer, vdecoder, vconv, vsink, adecoder, aconv, asink, vqueue, aqueue, effect,NULL);

  // Elaces no dinámicos
  gst_element_link (source, demuxer);
  gst_element_link_many (vdecoder, vqueue , effect, vconv, vsink, NULL);
  gst_element_link_many (adecoder, aqueue ,aconv, asink, NULL);

  // Enlaces dinámicos
  g_signal_connect (demuxer, "pad-added", G_CALLBACK (on_pad_added), vdecoder);
  g_signal_connect (demuxer, "pad-added", G_CALLBACK (on_pad_added), adecoder);

  // Se inicia la pipeline
  g_print ("Now playing: %s\n", argv[1]);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);


  /* Iterate */
  g_print ("Running...\n");
  g_main_loop_run (loop);


  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);

  return 0;
}
