from enum import Enum
import cv2
import socket
import matplotlib.pyplot as plt
import cvlib as cv
import urllib.request
import numpy as np
from cvlib.object_detection import draw_bbox
import concurrent.futures

url = 'http://172.20.10.2/cam-hi.jpg'
im = None

HOST = "something"
PORT = 3000


class TrashState(Enum):
    TRASH = 0
    ELECTRONIC = 1
    RECYCLING = 2
    COMPOST = 3


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
        determineStates(label)

        cv2.imshow('detection', im)
        key = cv2.waitKey(5)
        if key == ord('q'):
            break

    cv2.destroyAllWindows()


def determineStates(label):  # the server is assumed to be active
    compost_states = ["banana", "apple", "sandwich", "orange",
                      "broccoli", "carrot", "hot dog", "pizza", "donut", "cake"]
    electronic_states = ["laptop", "mouse", "remote",
                         "keyboard", "cell phone", "microwave", "oven", "toaster"]
    recycling_states = ["wine glass", "cup", "fork", "knife", "spoon", "bowl"]
    total_states = compost_states + electronic_states + recycling_states
    for state in label:
        if state in total_states:
            if state in compost_states:
                sendSignal(conn, TrashState.COMPOST)
            elif state in electronic_states:
                sendSignal(conn, TrashState.ELECTRONIC)
            elif state in recycling_states:
                sendSignal(conn, TrashState.RECYCLING)
            else:
                sendSignal(conn, TrashState.TRASH)


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
