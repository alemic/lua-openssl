/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2012 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/
/*=========================================================================*\
* crl routines
* lua-openssl toolkit
*
* This product includes PHP software, freely available from <http://www.php.net/software/>
* Author:  george zhao <zhaozg(at)gmail.com>
\*=========================================================================*/
#include "openssl.h"


int		X509_CRL_cmp(const X509_CRL *a, const X509_CRL *b);
int		X509_CRL_match(const X509_CRL *a, const X509_CRL *b);

int openssl_get_revoke_reason(const char*s){

	int reason = -1;
	int i;
	for (i=0; i<reason_num; i++)
	{
		if(strcasecmp(s,reason_flags[i].lname)==0 || strcasecmp(s,reason_flags[i].sname)==0)
		{
			reason = reason_flags[i].bitnum;
			break;
		}
	}
	return reason;
}

X509_REVOKED *openssl_X509_REVOKED(lua_State*L, int snidx, int timeidx, int reasonidx) {
    X509_REVOKED *revoked = X509_REVOKED_new();
    const char* serial = luaL_checkstring(L, snidx);
    BIGNUM * bn = NULL;
    ASN1_TIME *tm = NULL;
    int reason = 0;
    ASN1_INTEGER *it = NULL;


    if(!BN_hex2bn(&bn, serial))
    {
        goto end;
    };

    if(lua_isnumber(L,timeidx) || lua_isnoneornil(L, timeidx))
    {
        time_t t;
        time(&t);
        t = luaL_optinteger(L, 3, (lua_Integer)t);
        tm = ASN1_TIME_new();
        ASN1_TIME_set(tm,t);
    } else if(lua_isstring(L, timeidx))
    {

    } else {
        goto end;
    }

    if(lua_isnumber(L, reasonidx) || lua_isnoneornil(L, reasonidx))
    {
        reason = luaL_optinteger(L, reasonidx, 0);
        if(reason < 0 || reason >= reason_num) {
            goto end;
        }

    } else if(lua_isstring(L, reasonidx))
    {
        const char* s = lua_tostring(L, reasonidx);
        reason = openssl_get_revoke_reason(s);
        if(reason < 0 || reason >= reason_num) {
            goto end;
        }
    } else
    {
        goto end;
    };

    it = BN_to_ASN1_INTEGER(bn,NULL);
    X509_REVOKED_set_revocationDate(revoked, tm);
    X509_REVOKED_set_serialNumber(revoked, it);
#if OPENSSL_VERSION_NUMBER > 0x10000000L
    revoked->reason = reason;
#else
	/*
    {
        ASN1_ENUMERATED * e = ASN1_ENUMERATED_new();
		X509_EXTENSION * ext = X509_EXTENSION_new();

        ASN1_ENUMERATED_set(e, reason);

        X509_EXTENSION_set_object(ext, OBJ_nid2obj(NID_crl_reason));
        X509_EXTENSION_set_data(ext,e);

        if(!revoked->extensions)
            revoked->extensions = sk_X509_EXTENSION_new_null();
		
		X509_REVOKED_add_ext()
        sk_X509_REVOKED_push(revoked->extensions,ext);

        X509_EXTENSION_free(ext);
        ASN1_ENUMERATED_free(e);
    }
	*/
#endif

    ASN1_TIME_free(tm);
    ASN1_INTEGER_free(it);
    BN_free(bn);

    return revoked;
end:
    X509_REVOKED_free(revoked);
    ASN1_TIME_free(tm);
    ASN1_INTEGER_free(it);
    BN_free(bn);
    return NULL;
}

LUA_FUNCTION(openssl_crl_new) {
    X509* x509 = CHECK_OBJECT(1, X509, "openssl.x509");
    time_t lastUpdate = luaL_checkinteger(L,2);
    time_t nextUpdate = luaL_checkinteger(L,3);
	long version;
    X509_CRL * crl = NULL;
    ASN1_TIME *ltm,*ntm;

    if(!lua_isnoneornil(L,4))
        luaL_checktype(L, 4, LUA_TTABLE);
	version = luaL_optint(L, 5, 1);

    crl = X509_CRL_new();
    X509_CRL_set_version(crl, version);
    X509_CRL_set_issuer_name(crl, x509->cert_info->issuer);
    ltm = ASN1_TIME_new();
    ASN1_TIME_set(ltm, lastUpdate);
    ntm = ASN1_TIME_new();
    ASN1_TIME_set(ntm, nextUpdate);
    X509_CRL_set_lastUpdate(crl, ltm);
    X509_CRL_set_nextUpdate(crl, ntm);
    if ( lua_istable(L,4) ) {
        int n = lua_objlen(L, 4);
        int i = 0;
        for (i=0; i<n; i++)
        {
            lua_rawgeti(L, 4, i+1);
            if(lua_istable(L,-1))
            {
                X509_REVOKED *revoked;
                lua_getfield(L, -1, "reason");
                lua_getfield(L, -2, "time");
                lua_getfield(L, -3, "sn");

                revoked = openssl_X509_REVOKED(L, -1, -2, -3);
                if(revoked) {
					X509_CRL_add0_revoked(crl,revoked);
                }
                lua_pop(L, 3);
            }
            lua_pop(L,1);
        }
    }
    ASN1_TIME_free(ltm);
    ASN1_TIME_free(ntm);
    PUSH_OBJECT(crl,"openssl.x509_crl");
    return 1;
}

LUA_FUNCTION(openssl_crl_read) {
    size_t len;
    char* dat = (char*)luaL_checklstring(L, 1, &len);
    BIO *in = BIO_new_mem_buf(dat, len);

    X509_CRL *crl = PEM_read_bio_X509_CRL(in, NULL,NULL,NULL);

    if(!crl)
    {
        BIO_reset(in);
        crl = d2i_X509_CRL_bio(in,NULL);
    }
    BIO_free(in);

    if(crl)
    {
        PUSH_OBJECT(crl,"openssl.x509_crl");
        return 1;
    }
    return 0;
}

LUA_FUNCTION(openssl_crl_set_version) {
    X509_CRL *crl = CHECK_OBJECT(1, X509_CRL, "openssl.x509_crl");
    long version = luaL_optinteger(L,2, 0);
    int ret = X509_CRL_set_version(crl, version);
    if(ret==0 || ret==1) {
        lua_pushboolean(L,ret);
    } else
        lua_pushnil(L);
    return 1;
}

LUA_FUNCTION(openssl_crl_set_issuer) {
    X509_CRL *crl = CHECK_OBJECT(1, X509_CRL, "openssl.x509_crl");
    X509* x509 = CHECK_OBJECT(2, X509, "openssl.x509");

    int ret = X509_CRL_set_issuer_name(crl, x509->cert_info->issuer);
    if(ret==0 || ret==1) {
        lua_pushboolean(L,ret);
    } else
        lua_pushnil(L);
    return 1;
}

LUA_FUNCTION(openssl_crl_set_updatetime) {
    X509_CRL *crl = CHECK_OBJECT(1, X509_CRL, "openssl.x509_crl");
    ASN1_TIME *ltm, *ntm;
    int ret = 0;

    time_t last, next;
    time(&last);
    last = luaL_optinteger(L, 2, (lua_Integer)last);
    next = luaL_optinteger(L, 3, (lua_Integer)last+7*24*3600);

    ltm = ASN1_TIME_new();
    ASN1_TIME_set(ltm, last);
    ntm = ASN1_TIME_new();
    ASN1_TIME_set(ntm, next);

    ret = X509_CRL_set_lastUpdate(crl, ltm);
    if(ret==1)
        X509_CRL_set_nextUpdate(crl, ntm);

    if(ret==0 || ret==1) {
        lua_pushboolean(L,ret);
    } else
        lua_pushnil(L);
    return 1;
}

LUA_FUNCTION(openssl_crl_sort) {
    X509_CRL *crl = CHECK_OBJECT(1, X509_CRL, "openssl.x509_crl");
    int ret = X509_CRL_sort(crl);
    if(ret==0 || ret==1) {
        lua_pushboolean(L,ret);
    } else
        lua_pushnil(L);
    return 1;
}

LUA_FUNCTION(openssl_crl_verify) {
    X509_CRL *crl = CHECK_OBJECT(1, X509_CRL, "openssl.x509_crl");
    X509* cacert = CHECK_OBJECT(2, X509, "openssl.x509");


    int ret = X509_CRL_verify(crl, cacert->cert_info->key->pkey);
    if(ret==0 || ret==1) {
        lua_pushboolean(L,ret);
    } else
        lua_pushnil(L);
    return 1;
}

LUA_FUNCTION(openssl_crl_sign) {
    X509_CRL *crl = CHECK_OBJECT(1, X509_CRL, "openssl.x509_crl");
    EVP_PKEY *key = CHECK_OBJECT(2, EVP_PKEY, "openssl.evp_pkey");
    const EVP_MD *md = lua_isuserdata(L,3) ? CHECK_OBJECT(3, EVP_MD, "openssl.evp_digest")
                       : EVP_get_digestbyname(luaL_optstring(L, 3, "sha1WithRSAEncryption"));
    int ret = 0;

    if(!md)
        luaL_error(L,"#3 paramater must be openssl.evp_digest or a valid digest alg name");
	
	X509_CRL_sort(crl);
    ret = X509_CRL_sign(crl, key, md);
    lua_pushboolean(L,ret);
    return 1;

}

LUA_FUNCTION(openssl_crl_add_revocked) {
    X509_CRL *crl = CHECK_OBJECT(1, X509_CRL, "openssl.x509_crl");
    int serailidx = 2;
    int timeidx = 3;
    int reasonidx = 4;
    int ret = 0;
    X509_REVOKED* revoked = openssl_X509_REVOKED(L, serailidx, timeidx, reasonidx);
    ret = X509_CRL_add0_revoked(crl,revoked);
    lua_pushboolean(L,ret);
    return 1;
}

LUA_FUNCTION(openssl_crl_parse) {
    X509_CRL *crl = CHECK_OBJECT(1, X509_CRL, "openssl.x509_crl");
    int useshortnames = lua_isnoneornil(L,2)?0:lua_toboolean(L,2);
    BIO* bio = BIO_new(BIO_s_mem());
    int i,n;
    ASN1_INTEGER *crlnum;

    lua_newtable(L);
    lua_pushinteger(L, X509_CRL_get_version(crl));
    lua_setfield(L, -2, "version");

    /* hash as used in CA directories to lookup cert by subject name */
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%08lx", X509_NAME_hash(X509_CRL_get_issuer(crl)));
        lua_pushstring(L,buf);
        lua_setfield(L,-2,"hash");
    }
#if 1
    {
	const EVP_MD *digest = EVP_get_digestbyname("SHA1");
        unsigned char md[EVP_MAX_MD_SIZE];
	n = sizeof(md);

        if (X509_CRL_digest(crl,digest,md,(unsigned int*)&n))
        {
		lua_newtable(L);
		lua_pushstring(L, OBJ_nid2sn(EVP_MD_type(digest)));
		lua_setfield(L,-2,"alg");
		lua_pushlstring(L,(const char*)md,n);
		lua_setfield(L,-2,"hash");
		lua_setfield(L,-2,"fingerprint");
        }
    }
#endif

    add_assoc_name_entry(L, "issuer", 	X509_CRL_get_issuer(crl), useshortnames);

    ADD_ASSOC_ASN1_TIME(bio, X509_CRL_get_lastUpdate(crl), "lastUpdate");
    ADD_ASSOC_ASN1_TIME(bio, X509_CRL_get_nextUpdate(crl), "nextUpdate");

    i = OBJ_obj2nid(crl->crl->sig_alg->algorithm);
    lua_pushstring(L, i==NID_undef ? "NONE": OBJ_nid2ln(i));
    lua_setfield(L, -2, "sig_alg");

    crlnum = X509_CRL_get_ext_d2i(crl, NID_crl_number,NULL, NULL);
    if(crlnum) {
        i2a_ASN1_INTEGER(bio, crlnum);
        ASN1_INTEGER_free(crlnum);
        ADD_ASSOC_BIO(bio,"crl_number");
    }

    add_assoc_x509_extension(L, "extensions", crl->crl->extensions, bio);

        n = sk_X509_REVOKED_num(crl->crl->revoked);
        lua_newtable(L);
        for (i=0; i<n; i++)
        {
            X509_REVOKED *revoked = sk_X509_REVOKED_value(crl->crl->revoked,i);
            lua_newtable(L);

#if OPENSSL_VERSION_NUMBER > 0x10000000L
            lua_pushstring(L,reason_flags[revoked->reason].lname);
            lua_setfield(L,-2,"CRLReason");
#else
            {
                int crit = 0;
                void* reason = X509_REVOKED_get_ext_d2i(revoked, NID_crl_reason,&crit, NULL);

                lua_pushstring(L,reason_flags[ASN1_ENUMERATED_get(reason)].lname);
                lua_setfield(L,-2,"CRLReason");
                ASN1_ENUMERATED_free(reason);
            }
#endif

            ADD_ASSOC_ASN1(ASN1_INTEGER, bio, revoked->serialNumber,"serialNumber");
            ADD_ASSOC_ASN1_TIME(bio, revoked->revocationDate, "revocationDate");

            add_assoc_x509_extension(L, "extensions", revoked->extensions, bio);

            lua_rawseti(L, -2, i+1);
        }

        lua_setfield(L,-2, "revoked");

    BIO_free(bio);

    return 1;
}

LUA_FUNCTION(openssl_crl_tostring) {
    X509_CRL *crl = CHECK_OBJECT(1, X509_CRL, "openssl.x509_crl");
    lua_pushfstring(L,"openssl.x509_crl:%p",crl);
    return 1;
}


LUA_FUNCTION(openssl_crl_free) {
    X509_CRL *crl = CHECK_OBJECT(1, X509_CRL, "openssl.x509_crl");
    X509_CRL_free(crl);
    return 0;
}


LUA_FUNCTION(openssl_crl_export)
{
	int pem, notext;
	X509_CRL * crl = CHECK_OBJECT(1,X509_CRL,"openssl.x509_crl");
	int top = lua_gettop(L);
	BIO* bio_out = NULL;

	pem = top > 1 ? lua_toboolean(L, 2) : 1;
	notext = (pem && top>2) ? lua_toboolean(L,3):1;

	bio_out	 = BIO_new(BIO_s_mem());
	if (pem) {
		if (!notext) {
			X509_CRL_print(bio_out, crl);
		}

		if (PEM_write_bio_X509_CRL(bio_out, crl))  {
			BUF_MEM *bio_buf;
			BIO_get_mem_ptr(bio_out, &bio_buf);
			lua_pushlstring(L,bio_buf->data, bio_buf->length);
		} else
			lua_pushnil(L);
	} else
	{
		if(i2d_X509_CRL_bio(bio_out, crl)) {
			BUF_MEM *bio_buf;
			BIO_get_mem_ptr(bio_out, &bio_buf);
			lua_pushlstring(L,bio_buf->data, bio_buf->length);
		} else
			lua_pushnil(L);
	}

	BIO_free(bio_out);
	return 1;
}

static luaL_Reg crl_funcs[] = {
    {"sort",	openssl_crl_sort},
    {"verify",	openssl_crl_verify},
    {"sign",	openssl_crl_sign},
	{"export",	openssl_crl_export},

    {"set_version",		openssl_crl_set_version		},
    {"set_update_time",	openssl_crl_set_updatetime	},
    {"set_issuer",		openssl_crl_set_issuer		},
    {"add_revocked",	openssl_crl_add_revocked	},

    {"parse",			openssl_crl_parse			},


    {"__tostring",		openssl_crl_tostring	},
    {"__gc",			openssl_crl_free	},

    {NULL,	NULL}
};


LUA_FUNCTION(openssl_register_crl) {
    auxiliar_newclass(L,"openssl.x509_crl", crl_funcs);
    return 0;
}
