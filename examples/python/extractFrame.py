'''
    Author: Chang Min Park
    Last Update: 06/05/19
'''
from OpenGL.GL import *

class FrameExtractor:
    
    # constructor
    def __init__(self, width, height):
        self.width = width
        self.height = height

    def extractTo(self, filePath):
        glReadBuffer(GL_FRONT)
        data = glReadPixels(0, 0, self.width, self.height, GL_BGR, GL_UNSIGNED_BYTE)
        f = open(filePath, 'wb')
        f.write(data)

