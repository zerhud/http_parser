{ pkgs ? import<nixos-unstable>{} }:
let
  stdenv = pkgs.gcc11Stdenv;
in stdenv.mkDerivation rec {
  name = "std_http_utils";
  src = pkgs.fetchFromGitHub {
    repo = name;
    owner = "zerhud";
    rev = "c279bcad4955968f32f73768ce69aaf25c190506";
    sha256 = "0rs9bxxrw4wscf4a8yl776a8g880m5gcm75q06yx2cn3lw2b7v22";
  };
  nativeBuildInputs = with pkgs; [ qtcreator cmake ninja ];
  buildInputs = with pkgs; [ boost17x ];
  CTEST_OUTPUT_ON_FAILURE=1;
}
