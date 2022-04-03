from enum import Enum
import cv2
import socket
import matplotlib.pyplot as plt
import cvlib as cv
import urllib.request
import numpy as np
from cvlib.object_detection import draw_bbox
import concurrent.futures
import serial
import time
arduino = serial.Serial(port='COM4', baudrate=115200, timeout=.1)

url = 'http://172.20.10.2/cam-hi.jpg'
im = None

HOST = "something"
PORT = 3000

first_frame = 1


class TrashState(Enum):
    RECYCLING = 0
    COMPOST = 1
    ELECTRONIC = 2
    TRASH = 3

class LedState(Enum):
    LED_PENDING = 1,
    LED_APPROVE = 2,


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
    cv2.namedWindow("detection", cv2.WINDOW_AUTOSIZE)
    while True:
        img_resp = urllib.request.urlopen(url)
        imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
        im = cv2.imdecode(imgnp, -1)

        bbox, label, conf = cv.detect_common_objects(im)
        im = draw_bbox(im, bbox, label, conf)
        if diff(im):
            determineStates(label)

        cv2.imshow('detection', im)
        key = cv2.waitKey(5)
        if key == ord('q'):
            break

    cv2.destroyAllWindows()

def diff(im):
    if first_frame:
        before = np.array(im)
        first_frame = 0
    threshold = 30
    now = np.array(im)
    mse = np.mean((now - before)**2)
    print(mse)
    before = now

    if  mse > threshold:
        return True
    return False



def determineStates(label): 
    compost_states = ["banana", "apple", "sandwich", "orange",
                      "broccoli", "carrot", "hot dog", "pizza", "donut", "cake"]
    electronic_states = ["laptop", "mouse", "remote",
                         "keyboard", "cell phone", "microwave", "oven", "toaster"]
    recycling_states = ["wine glass", "cup", "fork", "knife", "spoon", "bowl"]
    total_states = compost_states + electronic_states + recycling_states
    if len(label) == 0:
        arduino.write(bytes('0' + str(LedState.LED_PENDING), 'utf-8'))
        return
    for state in label:
        if state in total_states:
            if state in compost_states:
                det_state = TrashState.COMPOST
            elif state in electronic_states:
                det_state = TrashState.ELECTRONIC
            elif state in recycling_states:
                det_state = TrashState.RECYCLING
        else:
            det_state = TrashState.TRASH
    arduino.write(bytes(str(det_state) + str(LedState.LED_APPROVE), 'utf-8'))


def connectToServer():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))  # connect to server
        s.listen()
        c, a = s.accept()

        return c, a


def sendSignal(c, state):  # sends the enum signal to the server
    if not state:
        return
    with c:
        c.sendall(state)


if __name__ == '__main__':
    print("started")
    global conn, addr
    conn, addr = connectToServer()
    with concurrent.futures.ProcessPoolExecutor() as executer:
        f1 = executer.submit(run1)
        f2 = executer.submit(run2)