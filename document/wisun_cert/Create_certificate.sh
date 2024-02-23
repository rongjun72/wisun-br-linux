# delete all exist *.pem file
rm -f *.c
rm -f *.pem
rm -f *.pem
rm -f *.old
rm -f *.txt
rm -f *.atr
rm -f *.attr
rm -f serial

# Create the certificate database, used to track created certificates.
touch certdb.txt

# Generate a Certificate Signing Request (CSR) for the root CA.
openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -keyout root_ca_key.pem -out root_ca_req.pem -config openssl-wisun.conf
# Self-sign the root CA.
openssl ca -selfsign -rand_serial -keyfile root_ca_key.pem -in root_ca_req.pem -out root_ca_cert.pem -notext -extensions v3_root_ca -config openssl-wisun.conf -subj "/CN=Wi-SUN Demo Root CA"


# Generate a CSR for the intermediate 1 CA.
openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -keyout interm1_ca_key.pem -out interm1_ca_req.pem -config openssl-wisun.conf
# Sign the intermediate 1 CA with the root CA.
openssl ca -rand_serial -cert root_ca_cert.pem -keyfile root_ca_key.pem -in interm1_ca_req.pem -out interm1_ca_cert.pem -notext -extensions v3_ca1 -config openssl-wisun.conf -subj "/CN=Wi-SUN Demo Intermediate 1 CA"


# Generate a CSR for the intermediate 2 CA.
openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -keyout interm2_ca_key.pem -out interm2_ca_req.pem -config openssl-wisun.conf
# Sign the intermediate 2 CA with the intermediate 1 CA.
openssl ca -rand_serial -cert interm1_ca_cert.pem -keyfile interm1_ca_key.pem -in interm2_ca_req.pem -out interm2_ca_cert.pem -notext -extensions v3_ca2 -config openssl-wisun.conf -subj "/CN=Wi-SUN Demo Intermediate 2 CA"


# Generate a CSR for the border router.
openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -keyout br_key.pem -out br_req.pem -config openssl-wisun.conf
# Sign the border router certificate with the desired CA.
openssl ca -rand_serial -cert interm2_ca_cert.pem -keyfile interm2_ca_key.pem -in br_req.pem -out br_cert.pem -notext -extensions v3_br -config openssl-wisun.conf -subj "/CN=Wi-SUN Demo Border Router"


# Generate a CSR for the device.
openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -keyout router_key.pem -out router_req.pem -config openssl-wisun.conf
# Sign the device certificate with the desired CA.
openssl ca -rand_serial -cert interm2_ca_cert.pem -keyfile interm2_ca_key.pem -in router_req.pem -out router_cert.pem -notext -extensions v3_device -config openssl-wisun.conf -subj "/CN=Wi-SUN Demo Device"


# ----------------------------------------------------------------
function generate_c_var
{
    #input params:
    # $1: target c filename
    # $2: input pem filename 
    # $3: var name in c file
    echo "const uint8_t $3[] = {" >> $1;
    lines=$(sed -n '$=' $2)
    for index in $(seq 1 $lines)
    do
        echo "    \"$(sed -n "${index}p" $2)\\r\\n\"" >> $1
    done
    echo "};" >> $1
    echo "" >> $1
}

function concatenate_pem
{
    #input params:
    # $1: target pem filename
    # $2: input pem filename 
    lines=$(sed -n '$=' $1)
    for index in $(seq 1 $lines)
    do
        echo "$(sed -n "${index}p" $1)" >> $2
    done
}

#touch pem_2_Cvars.c
#echo "#ifndef WISUN_TEST_CERTIFICATES_H_" >> pem_2_Cvars.c
#echo "#define WISUN_TEST_CERTIFICATES_H_" >> pem_2_Cvars.c
#echo "#include <stdint.h>" >> pem_2_Cvars.c
#echo "" >> pem_2_Cvars.c
#generate_c_var pem_2_Cvars.c root_ca_cert.pem       WISUN_ROOT_CERTIFICATE
#generate_c_var pem_2_Cvars.c br_cert.pem            WISUN_SERVER_CERTIFICATE
#generate_c_var pem_2_Cvars.c br_key.pem             WISUN_SERVER_KEY
#generate_c_var pem_2_Cvars.c router_cert.pem        WISUN_CLIENT_CERTIFICATE
#generate_c_var pem_2_Cvars.c router_key.pem         WISUN_CLIENT_KEY
#generate_c_var pem_2_Cvars.c interm1_ca_cert.pem    WISUN_CA1_CERTIFICATE
#generate_c_var pem_2_Cvars.c interm1_ca_key.pem     WISUN_CA1_CKEY
#generate_c_var pem_2_Cvars.c interm2_ca_cert.pem    WISUN_CA2_CERTIFICATE
#generate_c_var pem_2_Cvars.c interm2_ca_key.pem     WISUN_CA2_CKEY
#echo "#endif /* WISUN_TEST_CERTIFICATES_H_ */" >> pem_2_Cvars.c

touch ca_cert.pem
concatenate_pem root_ca_cert.pem    ca_cert.pem
concatenate_pem interm1_ca_cert.pem ca_cert.pem
concatenate_pem interm2_ca_cert.pem ca_cert.pem
touch pem_2_vars2.c
echo "#ifndef WISUN_TEST_CERTIFICATES_H_" >> pem_2_vars2.c
echo "#define WISUN_TEST_CERTIFICATES_H_" >> pem_2_vars2.c
echo "#include <stdint.h>" >> pem_2_vars2.c
echo "" >> pem_2_vars2.c
generate_c_var pem_2_vars2.c ca_cert.pem            WISUN_ROOT_CERTIFICATE
generate_c_var pem_2_vars2.c router_cert.pem        WISUN_CLIENT_CERTIFICATE
generate_c_var pem_2_vars2.c router_key.pem         WISUN_CLIENT_KEY
echo "#endif /* WISUN_TEST_CERTIFICATES_H_ */" >> pem_2_Cvars.c


# copy/replace pem files in RaspPi4B
scp br_key.pem          cmt@192.168.31.97:/home/cmt/Git_repository/wisun-br-rong/examples/
scp br_cert.pem         cmt@192.168.31.97:/home/cmt/Git_repository/wisun-br-rong/examples/
#scp interm2_ca_key.pem  cmt@192.168.31.97:/home/cmt/Git_repository/wisun-br-rong/examples/
#scp interm2_ca_cert.pem cmt@192.168.31.97:/home/cmt/Git_repository/wisun-br-rong/examples/
#scp interm1_ca_key.pem  cmt@192.168.31.97:/home/cmt/Git_repository/wisun-br-rong/examples/
#scp interm1_ca_cert.pem cmt@192.168.31.97:/home/cmt/Git_repository/wisun-br-rong/examples/
#scp root_ca_cert.pem    cmt@192.168.31.97:/home/cmt/Git_repository/wisun-br-rong/examples/ca_cert.pem
scp ca_cert.pem    cmt@192.168.31.97:/home/cmt/Git_repository/wisun-br-rong/examples/ca_cert.pem
