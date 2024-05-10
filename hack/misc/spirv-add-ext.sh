#!/bin/bash

{
set -euo pipefail

# see: https://blog.danslimmon.com/2019/07/15/do-nothing-scripting-the-key-to-gradual-automation/
# and: https://github.com/KhronosGroup/SPIRV-Guide/blob/main/chapters/creating_extension.md


ENUM_TOP=4294967295
OPCODE_TOP=65535
# cf. https://github.com/KhronosGroup/SPIRV-Headers/blob/main/include/spirv/spir-v.xml

pause() {
	read -rp "${1-Enter to continue}..."
}

next_enum() {
	echo $(( ENUM_TOP-- ))
}

next_opcode() {
	echo $(( OPCODE_TOP-- ))
}

set -x
cd "$(dirname "${BASH_SOURCE[0]}")/../../wasm/SPIRV-Headers/include/spirv/unified1/"
set +x

echo "look in SPIRV-Headers/include/spirv/unified1/spirv.core.grammar.json"

# TODO skip if capability is aleady defined, as below?
cat <<EOF
** Add a Capability enum

find the end of the enumerants array that starts

	{
		"category" : "ValueEnum",
		"kind" : "Capability",
		"enumerants" : [
			{
				"enumerant" : "Matrix",
				"value" : 0,
				"version" : "1.0"
			},
		....

and add a value like:

	{
		"enumerant" : "DispatchTALVOS",
		"value" : $(next_enum),
		"extensions" : [ "SPV_TALVOS_dispatch" ],
		"version" : "None"
	}

NB: before submission, change the enumerant value to a registered range
    (registering ranges is in a different task)
EOF
pause

pause "** Likely add BuiltIn, Decoration, or other enums"

echo "** Possibly add new instructions"

[[ -v CLASS ]] || {
(
cat <<EOF
Choose (or add) a "class"

Existing classes:
 no. of
  insns|class name
EOF

<spirv.core.grammar.json \
	jq -r '.instructions[].class' \
	| sort | uniq -c | sort -rn
)
read -rp "Class: [Reserved] " CLASS
CLASS=${CLASS:-Reserved}
}

while ! <spirv.core.grammar.json \
	jq >/dev/null -ef /dev/fd/3 3<<-FILTER
.instruction_printing_class
	|map(.tag)
	|index("${CLASS}")
FILTER
do
	pause "ADDING TAG '$CLASS': add your tag to the instruction_printing_class key"
done


cat <<EOF
Add an instruction by adding something to the instructions array like:

	{
		"opname" : "OpDispatchTALVOS",
		"class"  : "$CLASS",
		"opcode" : $(next_opcode),
		"operands" : [
			{"todo!": ""}
		],
		"capabilities" : [ "DispatchTALVOS" ],
		"version" : "None"
	}

NB: Make sure your capability matches the one you added above.
EOF

pause

echo "Rebuild the headers by running the script (as described in the README) to generate the C header file"
echo " i.e. run \`./hack/misc/spirv-build-headers.sh\`"

pause


}
