Para ejecutar el programa hay que pasarle como único argumento el nombre 
(o la ruta si no se está en el mismo directorio) del vídeo.

gcc -Wall pipeline.c -o pipeline $(pkg-config --cflags --libs gstreamer-1.0)

./pipeline video.ogg