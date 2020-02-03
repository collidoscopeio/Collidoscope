#version 150 core

uniform vec2 uCloudCentre;
uniform float uRadius;
uniform float uDamping;

in vec2 iPosition;
in vec2 iDirection;
in vec2 iCloudCentre;
in float iLifeSpan;
in float iDamping;

out vec2  position;
out vec2  direction;
out vec2 cloudCentre;
out float lifeSpan;
out float damping;


void main()
{
  direction = iDirection;
  cloudCentre = iCloudCentre; // to carry the value over 
  position = iPosition + direction;

  if (iDamping > uDamping) // stop all those with dumping bigger than current dumping 
  {
    position = uCloudCentre;
    cloudCentre = uCloudCentre;

  }
  else if (length(position - iCloudCentre) > uRadius - iLifeSpan)
  {
    cloudCentre = uCloudCentre;
    position = uCloudCentre;
  }

  damping = iDamping;
  lifeSpan = iLifeSpan;
}

