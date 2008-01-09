#include "argument.h"


Argument::Argument()
{
}

Argument::~Argument()
{
}

QString Argument::name()
{
	return this->m_name;
}
QString Argument::direction()
{
	return this->m_direction;
}
QString Argument::relatedStateVariable()
{
	return this->m_relatedStateVariable;
}

void Argument::setName(QString name)
{
	this->m_name = name;
}
void Argument::setDirection(QString direction)
{
	this->m_direction = direction;
}
void Argument::setRelatedStateVariable(QString relatedStateVariable)
{
	this->m_relatedStateVariable = relatedStateVariable;
}
