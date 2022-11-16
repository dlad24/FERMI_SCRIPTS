import cv2
import numpy as np
import os

# load image

final = cv2.imread('2022_08_16-12_59_45_AM0.bmp',cv2.COLOR_BGR2GRAY)
# load mask as grayscale
try:
    for file in os.listdir():
        if (str(file)[-3:] != 'bmp'):
            continue
        if (str(file)) == 'baseline.jpg':
            continue
        image = cv2.imread(str(file),cv2.COLOR_BGR2GRAY)
        cv2.imshow('image',final)
        cv2.waitKey(0)
        if cv2.countNonZero(image) < 500:
            final = np.add(image,final)
            print(str(file))
except KeyboardInterrupt:
    print('broke')

final = cv2.cvtColor(final,cv2.COLOR_GRAY2BGR)
ret, thresh = cv2.threshold(final,100,255,cv2.THRESH_BINARY)
cv2.imshow('thresh',thresh)
##final[final==[255,255,255]] = [0,0,255]

##thresh[np.where((thresh==[255, 255, 255]).all(axis=2))] = [0, 0, 255]
cv2.imshow('final',thresh)
cv2.waitKey(0)

print(final)

baseline = cv2.imread('baseline.jpg',cv2.COLOR_GRAY2BGR)
print(final.shape)
print(baseline.shape)

overlay = cv2.addWeighted(baseline,0.4,thresh,0.4,0)
cv2.imshow('mask',overlay)
cv2.waitKey(0)

# save resulting masked image

cv2.imwrite('overlay.jpg',overlay)
