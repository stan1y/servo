openssl genrsa -out jwt.private.pem 4096
openssl rsa -in jwt.private.pem -pubout > jwt.public.pem
