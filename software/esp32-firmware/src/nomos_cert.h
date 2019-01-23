#ifndef __NOMOS_CERT_H__
#define __NOMOS_CERT_H__

/*  > openssl s_client -showcerts -connect membership.vanhack.ca:443 </dev/null
    The CA root cert is the last cert given in the chain of certs.

    CONNECTED(00000003)
    depth=2 O = Digital Signature Trust Co., CN = DST Root CA X3
    verify return:1
    depth=1 C = US, O = Let's Encrypt, CN = Let's Encrypt Authority X3
    verify return:1
    depth=0 CN = membership.vanhack.ca
    verify return:1
    ---
    Certificate chain
    0 s:/CN=membership.vanhack.ca
    i:/C=US/O=Let's Encrypt/CN=Let's Encrypt Authority X3
    -----BEGIN CERTIFICATE-----
    MIIFYTCCBEmgAwIBAgISAxJSckcNMNESWCbf9/xMmX37MA0GCSqGSIb3DQEBCwUA
    MEoxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MSMwIQYDVQQD
    ExpMZXQncyBFbmNyeXB0IEF1dGhvcml0eSBYMzAeFw0xODExMTUwNzUyNDVaFw0x
    OTAyMTMwNzUyNDVaMCAxHjAcBgNVBAMTFW1lbWJlcnNoaXAudmFuaGFjay5jYTCC
    ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALi7uDEP//2AvCRSXHQ74twa
    4ft+MmTWqU2I8Tc3eYm6NKZChYBVdABGhzPJ/ySavjoL7boLJe+AXXnJF4l5HDE7
    Q7pNzyGUld5uj0wwwS2K007llqR4xWqg+tcPnFcSsFjznXvPUzyAuta79qr3bFEz
    p+QEwRbgoly2wVDXljLBvTT60ki46dfvfpT6Z3ukMlt8fdpq89TexjrMseg6rzla
    4uQem5325T3OSC1fgj806nM4keQ8Bu/nKgjMeWWq9xqJKnE0FHUy1suBiFQXHTTr
    V4D4t84ur/xu+DC7B+HZI1U8OBhb+VhsASIrEyozyye7Xz2yOc66taRmz2ZuvecC
    AwEAAaOCAmkwggJlMA4GA1UdDwEB/wQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcD
    AQYIKwYBBQUHAwIwDAYDVR0TAQH/BAIwADAdBgNVHQ4EFgQUjmzOIroGpsroT/ly
    pyGAVSi8U8EwHwYDVR0jBBgwFoAUqEpqYwR93brm0Tm3pkVl7/Oo7KEwbwYIKwYB
    BQUHAQEEYzBhMC4GCCsGAQUFBzABhiJodHRwOi8vb2NzcC5pbnQteDMubGV0c2Vu
    Y3J5cHQub3JnMC8GCCsGAQUFBzAChiNodHRwOi8vY2VydC5pbnQteDMubGV0c2Vu
    Y3J5cHQub3JnLzAgBgNVHREEGTAXghVtZW1iZXJzaGlwLnZhbmhhY2suY2EwTAYD
    VR0gBEUwQzAIBgZngQwBAgEwNwYLKwYBBAGC3xMBAQEwKDAmBggrBgEFBQcCARYa
    aHR0cDovL2Nwcy5sZXRzZW5jcnlwdC5vcmcwggEDBgorBgEEAdZ5AgQCBIH0BIHx
    AO8AdQB0ftqDMa0zEJEhnM4lT0Jwwr/9XkIgCMY3NXnmEHvMVgAAAWcWkpmPAAAE
    AwBGMEQCIBAme8MS8HDj2iFmY4ePWwJEQ4ZtU1sDVkhUUkM38ByHAiBf2Cce0OIf
    vPPAoauVvbNHgZMnVj64xSZlm+m6kC6WKAB2AGPy283oO8wszwtyhCdXazOkjWF3
    j711pjixx2hUS9iNAAABZxaSmakAAAQDAEcwRQIhANPkyvgWW66CLbUtU5Vsmyu3
    HueXD/m9i81nzJtVLWhCAiBnc60CaUB6JNPBu5teteKoS7xqg/mpWWo5yynSLkI5
    VDANBgkqhkiG9w0BAQsFAAOCAQEAIElcfNijIMwuwMNahl+H3pyMoVOAlGMb1ksT
    d4OGpzJfDHGMlWho4TtBldlLjlkYlUIEZ8XLwufaHE455Ro1nJIDzmgJxsbGISs1
    VAqfksCHtJX2FEwO6R3uLhbB3JU+vUKFXQfoGmqu7STe2maq+fFy7yXPvjWAUQxf
    FEHnMS5j2cSM8NijeFHCDTiuxtJNzRt9eYcnQTMkoTZCMx9Q2/0WIM747PK8wgBx
    pM0pC7gJLzREVptAzPyZsU0PKMRFnH5wuk02Ruv+jqzhCfLBgyBq8vV8pf6FyRpf
    YrBQozSR/3kb5K7ZzWfEhzEjqzmhdyxrwNo7iHsOKP4iAO7FMg==
    -----END CERTIFICATE-----
    1 s:/C=US/O=Let's Encrypt/CN=Let's Encrypt Authority X3
    i:/O=Digital Signature Trust Co./CN=DST Root CA X3
    -----BEGIN CERTIFICATE-----
    MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/
    MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT
    DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow
    SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT
    GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC
    AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF
    q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8
    SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0
    Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA
    a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj
    /PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T
    AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG
    CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv
    bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k
    c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw
    VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC
    ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz
    MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu
    Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF
    AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo
    uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/
    wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu
    X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG
    PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6
    KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==
    -----END CERTIFICATE-----
    ---
    Server certificate
    subject=/CN=membership.vanhack.ca
    issuer=/C=US/O=Let's Encrypt/CN=Let's Encrypt Authority X3
    ---
    No client certificate CA names sent
    Peer signing digest: SHA512
    Server Temp Key: ECDH, P-256, 256 bits
    ---
*/

// uint8_t nomos_cert[] =
//     "-----BEGIN CERTIFICATE-----\n"
//     "MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/\n"
//     "MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"
//     "DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow\n"
//     "SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT\n"
//     "GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC\n"
//     "AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF\n"
//     "q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8\n"
//     "SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0\n"
//     "Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA\n"
//     "a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj\n"
//     "/PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T\n"
//     "AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG\n"
//     "CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv\n"
//     "bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k\n"
//     "c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw\n"
//     "VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC\n"
//     "ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz\n"
//     "MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu\n"
//     "Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF\n"
//     "AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo\n"
//     "uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/\n"
//     "wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu\n"
//     "X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG\n"
//     "PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6\n"
//     "KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==\n"
//     "-----END CERTIFICATE-----";

// Cert is embedded in the binary via platformio.ini
extern const uint8_t nomos_root_cert_pem_start[] asm("_binary_src_nomos_root_cert_pem_start");
extern const uint8_t nomos_root_cert_pem_end[] asm("_binary_src_nomos_root_cert_pem_end");

#endif //__NOMOS_CERT_H__
