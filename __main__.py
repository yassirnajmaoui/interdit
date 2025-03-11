#! /usr/bin/env python
import argparse
import sys
from viewer import *
from image_loader import *
import python_tools.iotools as ptio
import glob
import os

# TODO: remove dependency from python_tools

def main():
    parser = argparse.ArgumentParser(description='Plots a 3D volume')
    parser.add_argument('-i', '--image', metavar='i', type=str, nargs='+', help='filename of the image (if using DICOM, specify glob)')
    parser.add_argument('-f', '--format', metavar='f', type=str, nargs='+', help='format of the volume (dicom, f32, f64, nii, npy)')

    args = parser.parse_args()
    
    if args.format is None:
        args.format = ["sitk"]*len(args.image)

    if args.image is None or args.format is None or len(args.image) <= 0 or len(args.format) <= 0:
        print("Error: Specify at least an image with a corresponding format")
        exit()
    if len(args.format) != len(args.image):
        print("Error: Use the same number of images as the number of formats given")
        if args.format == "dcm" or args.format == "dicom":
            print("Warning: If you use bash and want to read a dicom, you'll need to specify a glob and"
                "you'll need to add quotes (\") around the glob to prevent bash from automatically expanding it")
        exit()
    
    
    images = []

    num_imgs = len(args.format)
    for i in range(num_imgs):
        if args.format[i] == "f32":
            img = ptio.DataFileRawd().load(args.image[i], dtype=np.float32)
        elif args.format[i] == "f64":
            img = ptio.DataFileRawd().load(args.image[i], dtype=np.float64)
        elif args.format[i] == "dicom" or args.format[i] == "dcm":
            slices = []
            sorted_glob = sorted(glob.glob(args.image[i]))
            for file_path in sorted_glob:
                if os.path.isfile(file_path):
                    slices.append(ptio.DataFileDicom().load(file_path))
            img = np.stack(slices, axis=0)
        elif args.format[i] == "sitk" or args.format[i] == "nii":
            img = ptio.DataFileSITK().load(args.image[i])
            if(len(img.shape) > 3):
                img = np.squeeze(img)
        elif args.format[i] == "npy" or args.format[i] == "np":
            img = ptio.DataFileNumpy().load(args.image[i])
            if(len(img.shape) > 3):
                img = np.squeeze(img)
        
        images.append(img)

    manager = VolumeViewerManager(images)
    sys.exit(manager.app.exec_())


if __name__ == "__main__":
    main()
