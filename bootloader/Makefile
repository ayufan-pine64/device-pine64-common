all: pine64/sun50i-a64-pine64-plus.dtb

%.dtb: %.dts
	dtc -Odtb -o "$@" "$<"
