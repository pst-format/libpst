#ifndef LZFU_H
#define LZFU_H

/** decompress lz compressed rtf data. The initial lz dictionary is preloaded
    with rtf specific data.
 * @param rtfcomp  pointer to the rtf compressed data
 * @param compsize size of the compressed data buffer
 * @param size     pointer to location to return size of the output buffer
 * @return         pointer to the output buffer containing the decompressed data.
 *                 The caller must free this buffer.
 */
char* pst_lzfu_decompress (char* rtfcomp, uint32_t compsize, size_t *size);

#endif
