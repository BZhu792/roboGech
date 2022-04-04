from enum import Enum
import cv2
import matplotlib.pyplot as plt
import cvlib as cv
import urllib.request
import numpy as np
from cvlib.object_detection import draw_bbox
import concurrent.futures
import serial

url = 'http://172.20.10.2/cam-hi.jpg'
im = None

first_frame = 1

compost_states = ["banana", "apple", "sandwich", "orange",
                    "broccoli", "carrot", "hot dog", "pizza", "donut", "cake"]
electronic_states = ["laptop", "mouse", "remote",
                        "keyboard", "cell phone", "microwave", "oven", "toaster"]
recycling_states = ["wine glass", "bottle", "cup", "fork", "knife", "spoon", "bowl"]

class TrashState(Enum):
    RECYCLING = 0
    COMPOST = 1
    ELECTRONIC = 2
    TRASH = 3
class LedState(Enum):
    LED_APPROVE = 2

def run1():
    cv2.namedWindow("live transmission", cv2.WINDOW_AUTOSIZE)
    while True:
        img_resp = urllib.request.urlopen(url)
        imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
        im = cv2.imdecode(imgnp, -1)

        cv2.imshow('live transmission', im)
        key = cv2.waitKey(5)
        if key == ord('q'):
            break

    cv2.destroyAllWindows()


def run2():
    arduino = serial.Serial(port='COM8', baudrate=115200, timeout=None)

    cv2.namedWindow("detection", cv2.WINDOW_AUTOSIZE)
    while True:
        img_resp = urllib.request.urlopen(url)
        imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
        im = cv2.imdecode(imgnp, -1)

        bbox, label, conf = cv.detect_common_objects(im)
        im = draw_bbox(im, bbox, label, conf)

        cv2.imshow('detection', im)
        key = cv2.waitKey(5)
        if key == ord('q'):
            break

        label = [value for value in label if value != "person"]
        label = [value for value in label if value != "backpack"]
        label = [value for value in label if value != "sports ball"]

        if len(label) > 0:
            if label[0] in compost_states:
                det_state = TrashState.COMPOST.value
            elif label[0] in electronic_states:
                det_state = TrashState.ELECTRONIC.value
            elif label[0] in recycling_states:
                det_state = TrashState.RECYCLING.value
            else:
                det_state = TrashState.TRASH.value
            arduino.write(str.encode(str(det_state)))

    cv2.destroyAllWindows()

if __name__ == '__main__':
    print("started")

    with concurrent.futures.ProcessPoolExecutor() as executer:
        f1 = executer.submit(run1)
        f2 = executer.submit(run2)