import sys
import os.path
import xml.etree.ElementTree as ET

inputDML = sys.argv[1]
varyProp = sys.argv[2]
outputDML = sys.argv[-1]

pathPrefix, f = os.path.split(inputDML)
fileName, ext = os.path.splitext(f)


for par in sys.argv[3:-1]:

    tree = ET.parse(inputDML)
    root = tree.getroot()

    if varyProp == "PARALLEL":
        for child in root:
            if child.tag == "optimization":
                for opt in child:
                    if opt.tag == "rule":
                        opt.attrib["threshold"] = par + "%"
                        print(opt.attrib["threshold"])

        tree.write(outputDML + "/" + fileName + "_" + par + ext)
