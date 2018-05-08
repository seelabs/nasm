;; extern int decode_hex256(char* in, char* out);

;; RDI is address of buf to decode (hex string). Must be 64 bytes.
;; RSI is the address of the output (must be 32 bytes)
;; If this is ported to windows, the calling convention is RCX, RDX for the first two params

section   .text

global decode_hex256

decode_hex256:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;; ymm{0,1}: input of hex digits

  vmovdqu ymm0, [rdi]
  vmovdqu ymm1, [rdi+32]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; upcase by clearing bit six
;;;
;;; ymm{0,1} from above
;;; ymm2 all bits set
;;; ymm3 upcase mask
;;; ymm4 all bytes '9'
;;; ymm{5,6} mask of all values > '9'
;;; ymm{7,8} mask of all values <= '9'
;;; ymm{9,10} mask to clear bit 6 for values > '9', leave other values alone

  vpcmpeqd ymm2, ymm2
  vpbroadcastb ymm3, [upcase_mask]
  vpbroadcastb ymm4, [ascii_9]

  vpcmpgtb ymm5, ymm0, ymm4     ; mask of all values > '9'
  vpcmpgtb ymm6, ymm1, ymm4
  vpxor ymm7, ymm5, ymm2       ; mask of all value <= '9'
  vpxor ymm8, ymm6, ymm2

  vpor ymm9, ymm7, ymm3       ; set the letter mask to one that will clear bit 6, upcasing it
  vpor ymm10, ymm8, ymm3
  vpand ymm0, ymm0, ymm9       ; apply the mask
  vpand ymm1, ymm1, ymm10

;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; any resulting hex digit greater than '9' and less than 'A' is an error
;;; ymm{0,1} from above
;;; ymm2 from above
;;; ymm3 all bytes 'A'
;;; ymm4 from above (all bytes '9')
;;; ymm{5,6} recomputed mask of values > '9'
;;; ymm{7,8} mask of values < 'A'
;;; ymm{9,10} mask of value > '9' and < 'A'
;;; ymm11 bit or of first 32 bytes and second 32 bytes of mask from ymm{9,10}

  vpbroadcastb ymm3, [ascii_A]
  vpcmpgtb ymm5, ymm0, ymm4
  vpcmpgtb ymm6, ymm1, ymm4
  vpcmpgtb ymm7, ymm3, ymm0
  vpcmpgtb ymm8, ymm3, ymm1
  vpand ymm9, ymm5, ymm7
  vpand ymm10, ymm6, ymm8
  vpor ymm11, ymm9, ymm10
  vptest ymm11, ymm2
  jnz .bad_hex_char

;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; convert the bytes to numeric values
;;; ymm{0,1} from above
;;; ymm2 from above
;;; ymm3 all bytes 48
;;; ymm4 all bytes 55
;;; ymm{5,6} from above
;;; ymm{7,8} values to subtract from input (either 48 or 55, depending if hex digit is '0'-'9' or 'A'-'F')

  vpbroadcastb ymm3, [fourtyeight]
  vpbroadcastb ymm4, [fiftyfive]
  vpblendvb ymm7, ymm3, ymm4, ymm5 ; select either the constants 48 or 55 depending on the mask in ymm4
  vpblendvb ymm8, ymm3, ymm4, ymm6

  vpsubb ymm0, ymm0, ymm7
  vpsubb ymm1, ymm1, ymm8

;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Error checking: if any of the bytes are greater than 15 (high nibble bits set)
;;; ymm{0,1} from above (numeric value of hex digit)
;;; ymm2 from above (all bits set)
;;; ymm3 all bytes 0xf0 (high nibble)
;;; ymm{4,5} masked high nibble of values
;;; ymm6 bit or of ymm{4,5}

  vpbroadcastb ymm3, [highnibble]
  vpand ymm4, ymm0, ymm3
  vpand ymm5, ymm1, ymm3
  vpor ymm6, ymm5, ymm4      ; ymm6 will be non-zero if any byte in ymm{0,1} is > 15
  vptest ymm6, ymm2
  jnz .bad_hex_char

;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; shift the even byte bits into the correct position
;;; ymm{0,1} from above
;;; ymm{3,4} shifted low 4 bits of even bytes into high 4 bits of odd bytes
;;; ymm5 shuffle pattern to shuffle the result into the bytes 8-15 in the high lane and bytes 0-7 in the low lane
;;;      Set bytes 0-8 to zero in the high lane and bytes 8-15 in the low lane
;;; ymm6 extracted high lane of ymm0 put into low lane of ymm6; extrated low lane of xmm1 put into high lane of ymm6

  vmovdqa ymm5, [shuffle]

  vpsllw ymm3, ymm0, 12
  vpsllw ymm4, ymm1, 12
  vpaddb ymm0, ymm0, ymm3       ; the odd bytes contain the correct bitpattern. The even bytes are junk
  vpaddb ymm1, ymm1, ymm4

  vpshufb ymm0, ymm0, ymm5
  vpshufb ymm1, ymm1, ymm5

  vextracti128 xmm6, ymm0, 1      ; Extract the high lane of ymm0 and put it into the low lane of ymm11
  vinserti128 ymm6, ymm6, xmm1, 1 ; Insert from the low lane of xmm1 into the high lane of ymm11
  vinserti128 ymm1, ymm1, xmm0, 0 ; Insert from the low lane from ymm0 and put it into the low land of xmm1

  ;; At this point:
  ;; ymm1 upper lane ==  upper lane of ymm1_original
  ;; ymm1 lower lane ==  lower lane of ymm0_original
  ;; ymm6 upper lane == lower lane of ymm1_original
  ;; ymm6 lower lane == upper lane of ymm0_original

  vpaddb ymm0, ymm1, ymm6      ; add them all together to get the 256 bit result

  ;; save the result
  vmovdqu [rsi], ymm0
  mov eax, 1
  vzeroupper
  ret

;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.bad_hex_char:
  xor eax,eax
  vzeroupper
  ret

section   .data align=32               ; align on 256 bit boundary for avx2 instructions
  ;; Shuffle pattern. N.B. Shuffles happen independently in the two 128 bit lanes
  ;; If bit seven is set (0x80), then a zero is written to the result byte
shuffle: db 1,3,5,7,9,11,13,15,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,  0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,1,3,5,7,9,11,13,15
ascii_9: db '9'
ascii_A: db 'A'
;; mask used to clear bit six
upcase_mask: db ~0x20
fourtyeight: db 48
fiftyfive: db 55
highnibble: db 0xf0
