#include "upnpKopete.h"
#include "applicationWidget.h"


UpnpKopete *UpnpKopete::uniqueInstance = NULL;

int kopeteCallbackEventHandler( Upnp_EventType EventType, void *Event, void *Cookie );


UpnpKopete::UpnpKopete()
{
	int ret;

	this->deviceType=QString("upnp:rootdevice");

	this->device_handle =-1;
	this->destPort = 0;

	this->m_mainDevices.setDeviceType(QString(""));	

	ret = UpnpInit(NULL,this->destPort);
	switch(ret)
	{
		case UPNP_E_SUCCESS: printf("The operation UpnpInit completed successfully\n");break;
		case UPNP_E_OUTOF_MEMORY: printf("Insufficient resources exist to initialize the SDK\n");
					UpnpFinish();break;
		case UPNP_E_INIT: printf("The SDK is already initialized\n");
					UpnpFinish();break;
		case UPNP_E_INIT_FAILED: printf("The SDK initialization failed for an unknown reason\n");
					UpnpFinish();break;
		case UPNP_E_SOCKET_BIND: printf("An error occurred binding a socket\n");
					UpnpFinish();break;
		case UPNP_E_LISTEN: printf("An error occurred listening to a socket\n");
					UpnpFinish();break;
		case UPNP_E_OUTOF_SOCKET: printf("An error ocurred creating a socket\n");
					UpnpFinish();break;
		case UPNP_E_INTERNAL_ERROR: printf("An internal error ocurred\n");
					UpnpFinish();break;
		default: UpnpFinish();break;
	}
	
	this->hostIp = QString(UpnpGetServerIpAddress());
	this->destPort = UpnpGetServerPort();
	printf("UPnP Initialized %s:%d\n",this->hostIp.toLatin1().data(),this->destPort);

	printf("Registering Control Point\n");
    	ret = UpnpRegisterClient(kopeteCallbackEventHandler,&device_handle, &device_handle );

	if( UPNP_E_SUCCESS != ret )
	{
        	printf( "Error registering Control point: %d\n", ret );
        	UpnpFinish();
        }
	printf("Control Point Registered\n");
}


UpnpKopete::~UpnpKopete()
{
	delete(this);
}

UpnpKopete* UpnpKopete::getInstance()
{
	if (uniqueInstance == NULL)
	{
		uniqueInstance = new UpnpKopete();
	}
	return uniqueInstance;
}

QString UpnpKopete::getHostIp()
{
	return this->hostIp;
}

unsigned short UpnpKopete::getDestPort()
{
	return this->destPort;
}

Device UpnpKopete::mainDevices()
{
	return this->m_mainDevices;
}

QString UpnpKopete::routeurLocation(){return this->m_routeurLocation;}
void UpnpKopete::setRouteurLocation(QString routeurLocation){this->m_routeurLocation=routeurLocation;}

QString UpnpKopete::descDoc(){return this->m_descDoc;}
void UpnpKopete::setDescDoc(QString url){this->m_descDoc = url;}

int UpnpKopete::researchDevice()
{
	int ret;
	ret = UpnpSearchAsync( device_handle, 1, this->deviceType.toLatin1().data() , NULL );
    	if( UPNP_E_SUCCESS != ret ) 
	{
        	printf("Error sending search request%d", ret);
		return RESEARCH_ERROR;
	}
    	return RESEARCH_SUCCESS;
}

void UpnpKopete::addDevice(IXML_Document * DescDoc,QString location)
{
	printf("#############ADD DEVICE###########\n");
	printf("location : %s\n",location.toLatin1().data());
	QString deviceType;
	QString friendlyName;
	QString manufacturer;
	QString manufacturerURL;
	QString modelName;
	QString UDN;
	QString modelDescription;
	QString modelNumber;
	QString serialNumber;
	QString presentationURL;
	QString UPC;
	QString DescDocURL;

	QString serviceType;
	QString serviceId;
	QString controlURL;
	QString eventSubURL;
	QString UrlDocXml;

	IXML_Document *parent = DescDoc;
	IXML_Node * nodeDevice;

	Upnp_SID SubsId;
	int ret;
	int timeOut = 1801;
	

	IXML_NodeList * deviceList = ixmlDocument_getElementsByTagName(parent,"deviceType");
	//checking if there is devicelists
	if(ixmlNodeList_length(deviceList) > 0)
	{
		int posDevice;
		for(int i = 0;i<ixmlNodeList_length(deviceList);i++)
		{
			if(strcmp(util_Xml_nodeValue(ixmlNodeList_item(deviceList,i)),"urn:schemas-upnp-org:device:WANConnectionDevice:1")==0)
			{
				nodeDevice = ixmlNodeList_item(deviceList,i);
				posDevice = i;
			}
		}
		//get the first devicelist
		IXML_Node* node = ixmlNodeList_item(deviceList,posDevice);
		//getting all devices from the devicelist
		IXML_NodeList* nChild = ixmlNode_getChildNodes(ixmlNode_getParentNode(node));
		for(int j = 0;j<ixmlNodeList_length(nChild);j++)
		{
			//getting all data list from device
			IXML_Node* n = ixmlNodeList_item(nChild,j);
			if(strcmp(ixmlNode_getNodeName(n),"deviceType")==0)
			{
				deviceType=QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"friendlyName")==0)
			{
				friendlyName = QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"manufacturer")==0)
			{
				manufacturer =QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"manufacturerURL")==0)
			{
				manufacturerURL = QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"modelName")==0)
			{
				modelName = QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"UDN")==0)
			{
				UDN = QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"modelDescription")==0)
			{
				modelDescription = QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"modelNumber")==0)
			{
				modelNumber = QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"serialNumber")==0)
			{
				serialNumber = QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"presentationURL")==0)
			{
				presentationURL = QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"UPC")==0)
			{
				UPC = QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"modelDescription")==0)
			{
				modelDescription = QString(util_Xml_nodeValue(n));
			}
			if(strcmp(ixmlNode_getNodeName(n),"serviceList")==0)
			{
				IXML_NodeList* nList = ixmlNode_getChildNodes(n);
				IXML_Node* nService = ixmlNodeList_item(nList,0);
				
				if(strcmp(ixmlNode_getNodeName(nService),"service")==0)
				{
					IXML_NodeList* nServiceList = ixmlNode_getChildNodes(nService);
					for(int m=0;m<ixmlNodeList_length(nServiceList);m++)
					{
						IXML_Node* nServiceItem = ixmlNodeList_item(nServiceList,m);
						if(strcmp(ixmlNode_getNodeName(nServiceItem),"serviceType")==0)
						{
							serviceType = QString(util_Xml_nodeValue(nServiceItem));
						}
						if(strcmp(ixmlNode_getNodeName(nServiceItem),"serviceId")==0)
						{
							serviceId = QString(util_Xml_nodeValue(nServiceItem));
						}
						if(strcmp(ixmlNode_getNodeName(nServiceItem),"controlURL")==0)
						{
							controlURL = QString(util_Xml_nodeValue(nServiceItem));
						}
						if(strcmp(ixmlNode_getNodeName(nServiceItem),"eventSubURL")==0)
						{
							eventSubURL = QString(util_Xml_nodeValue(nServiceItem));
						}
						if(strcmp(ixmlNode_getNodeName(nServiceItem),"SCPDURL")==0)
						{
							int indice = location.lastIndexOf (QString('/'),-1, Qt::CaseSensitive) ;
							
							UrlDocXml = location.left(indice);
							if(QString(util_Xml_nodeValue(nServiceItem)).at(0)!=QChar('/'))
							{
								UrlDocXml.push_back('/');
							}
							UrlDocXml.append(util_Xml_nodeValue(nServiceItem));		
						}
					}
				}
				
			}
		}
				
		Device underDevice = Device();
		underDevice.setDeviceType(deviceType);
		underDevice.setFriendlyName(friendlyName);
		underDevice.setManufacturer(manufacturer);
		underDevice.setManufacturerURL(manufacturerURL);
		underDevice.setModelName(modelName);
		underDevice.setUDN(UDN);
		underDevice.setModelDescription(modelDescription);
		underDevice.setModelNumber(modelNumber);
		underDevice.setSerialNumber(serialNumber);
		underDevice.setPresentationURL(presentationURL);
		underDevice.setUPC(UPC);
		underDevice.setDescDocURL(location);


		//adding device service
		Service service = Service();
		service.setServiceType(serviceType);
		service.setServiceId(serviceId);
		service.setControlURL(controlURL);
		service.setEventSubURL(eventSubURL);
		printf("urlDocService : %s\n",UrlDocXml.toLatin1().data());
		service.setXmlDocService(UrlDocXml);
		
		service.addAllActions();
		
		underDevice.addService(service);
		
		//subscribe device
		printf("---------SUBSCRIBE---------\n");
		char * relURL = (char *)malloc( strlen( location.toLatin1().data() ) + strlen( service.eventSubURL().toLatin1().data() ) + 1 );
		ret = UpnpResolveURL( location.toLatin1().data(), service.eventSubURL().toLatin1().data(), relURL );
                printf("ACTIONURL : %s\n",relURL);
		if( ret != UPNP_E_SUCCESS )		
		{
			printf("Erreur UpnpResolveURL : %d\n",ret);
		}
		ret = UpnpSubscribe( device_handle, relURL,&timeOut,SubsId );
                if( ret != UPNP_E_SUCCESS ) 
		{
                        printf("Erreur Subscribed to EventURL : %d\n",ret);
                } 

		//service.viewActionList();


		//add device in the list maindevice
		if(this->m_mainDevices.deviceType() == QString(""))
		{
			this->m_mainDevices = underDevice;
		}
		else
		{
			printf("Device main exist\n");
		}					
	}


}


void UpnpKopete::viewDevice()
{
	this->m_mainDevices.showDevice();
}


void UpnpKopete::sendAction(QString nameAction, QList<QString> paramNameAction,QList<QString> paramValueAction)
{
	bool find = false;
	int ret;
	IXML_Document *actionNode = NULL;
	
// 	Action action = NULL;
	if(this->m_mainDevices.deviceType() != QString(""))
	{
		for(int j=0;j<this->m_mainDevices.getListService().size() && !find;j++)
		{
			Service service_tmp=this->m_mainDevices.getListService().at(j);
			if(service_tmp.existAction(nameAction)==true)
			{
				if(paramNameAction.isEmpty())
				{
					printf("-------PAS de PARAM\n");
					actionNode = UpnpMakeAction( nameAction.toLatin1().data(), service_tmp.serviceType().toLatin1().data(), 0,
					NULL );
				}
				else
				{
					printf("-------PARAM\n");
					for( int k=0; k < paramNameAction.size(); k++ ) 
					{
						QString nameActionP = paramNameAction.at(k);
						QString valueActionP = paramValueAction.at(k);
					
						
						if( UpnpAddToAction
						( &actionNode, nameAction.toLatin1().data(), service_tmp.serviceType().toLatin1().data(),
						nameActionP.toLatin1().data(),valueActionP.toLatin1().data()) != UPNP_E_SUCCESS ) 
						{
							printf("Erreur UpnpAddToAction\n");
						//return -1; // TBD - BAD! leaves mutex locked
						}
					}
			
				}
				char * relURL = (char *)malloc( strlen( this->routeurLocation().toLatin1().data() ) + strlen( service_tmp.controlURL().toLatin1().data() ) + 1 );
		ret = UpnpResolveURL( this->routeurLocation().toLatin1().data(), service_tmp.controlURL().toLatin1().data(), relURL );
		if( ret != UPNP_E_SUCCESS )		
		{
			printf("Erreur UpnpResolveURL : %d\n",ret);
		}

				printf("DOCUMENT--------------\n xmlDoc : %s\n",relURL);
				printf("%s\n",ixmlPrintNode((IXML_Node*)actionNode));
				ret = UpnpSendActionAsync( 
					device_handle,relURL,
				 	service_tmp.serviceType().toLatin1().data(),
					NULL, 
					actionNode,
					kopeteCallbackEventHandler,
					NULL );				
				
				if( ret != UPNP_E_SUCCESS ) 
				{
					printf("Erreur UpnpSendActionAsync\n");
				}
				
				if( actionNode )
        				ixmlDocument_free( actionNode );
				
				find = true;
			}
		}
	}
}

void UpnpKopete::openPort(QString nameProtocol, int numPort)
{
	printf("---------------ACTION OPEN PORT----------------------\n");	
	QList<QString> paramNameAction;
	QList<QString> paramValueAction;
	//sendAction(QString("RequestConnection"),paramNameAction,paramValueAction);	

	char c_numPort[50];

	
	paramNameAction.append(QString("NewRemoteHost"));
	paramNameAction.append(QString("NewExternalPort"));
	paramNameAction.append(QString("NewProtocol"));
	paramNameAction.append(QString("NewInternalPort"));
	paramNameAction.append(QString("NewInternalClient"));
	paramNameAction.append(QString("NewEnabled"));
	paramNameAction.append(QString("NewPortMappingDescription"));
	paramNameAction.append(QString("NewLeaseDuration"));
	
	sprintf(c_numPort,"%d",numPort);

	
	paramValueAction.append(QString(""));
	paramValueAction.append(c_numPort);
	paramValueAction.append(QString("TCP"));
	paramValueAction.append(c_numPort);
	paramValueAction.append(this->hostIp);
	paramValueAction.append(QString("1"));
	paramValueAction.append(nameProtocol);
	paramValueAction.append(QString("0"));
	
	sendAction(QString("AddPortMapping"),paramNameAction,paramValueAction);
}

void UpnpKopete::deletePort(int numPort)
{
	printf("---------------ACTION DELETE PORT----------------------\n");	
	QList<QString> paramNameAction;
	QList<QString> paramValueAction;

	char c_numPort[50];

	paramNameAction.append(QString("NewRemoteHost"));
	paramNameAction.append(QString("NewExternalPort"));
	paramNameAction.append(QString("NewProtocol"));

	sprintf(c_numPort,"%d",numPort);

	paramValueAction.append(QString(""));
	paramValueAction.append(c_numPort);
	paramValueAction.append(QString("TCP"));

	sendAction(QString("DeletePortMapping"),paramNameAction,paramValueAction);
}

int kopeteCallbackEventHandler( Upnp_EventType EventType, void *Event, void *Cookie )
{
	int ret = 0;
 	UpnpKopete * getUpnp = UpnpKopete::getInstance();

	
    switch ( EventType ) {
            /*
               SSDP Stuff 
             */
        case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		printf("UPNP_DISCOVERY_ADVERTISEMENT_ALIVE\n");break;
        case UPNP_DISCOVERY_SEARCH_RESULT:
		{
			printf("UPNP_DISCOVERY_SEARCH_RESULT\n");
			struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)Event;

			IXML_Document *DescDoc = NULL;
                	int ret;

                	if( d_event->ErrCode != UPNP_E_SUCCESS ) 
			{
                    		printf( "Error in Discovery Callback -- %d",d_event->ErrCode );
                	}

                	if((ret = UpnpDownloadXmlDoc(d_event->Location,&DescDoc)) != UPNP_E_SUCCESS ) 
			{
                    		printf("Error obtaining device description from %s -- error = %d",d_event->Location, ret);
                	}
			else
			{
				if(getUpnp->mainDevices().deviceType() == QString(""))
				{
					getUpnp->setRouteurLocation(QString(d_event->Location));
					printf("Location URL %s\n",d_event->Location);
					getUpnp->setDescDoc(QString(d_event->Location));
					getUpnp->addDevice(DescDoc, d_event->Location);
                		}
			}

                	if( DescDoc )
                    		ixmlDocument_free( DescDoc );

			break;
		}
        case UPNP_DISCOVERY_SEARCH_TIMEOUT:
	{
		printf("UPNP_DISCOVERY_SEARCH_TIMEOUT\n");
		break;
	}
        case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
		printf("UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE\n");break;
        case UPNP_CONTROL_ACTION_COMPLETE:
		printf("UPNP_CONTROL_ACTION_COMPLETE\n");break;
        case UPNP_CONTROL_GET_VAR_COMPLETE:
		printf("UPNP_CONTROL_GET_VAR_COMPLETE\n");break;
        case UPNP_EVENT_RECEIVED:
		printf("UPNP_EVENT_RECEIVED\n");break;
        case UPNP_EVENT_SUBSCRIBE_COMPLETE:
		printf("UPNP_EVENT_SUBSCRIBE_COMPLETE\n");break;
        case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
		printf("UPNP_EVENT_UNSUBSCRIBE_COMPLETE\n");break;
        case UPNP_EVENT_RENEWAL_COMPLETE:
		printf("UPNP_EVENT_RENEWAL_COMPLETE\n");break;
        case UPNP_EVENT_AUTORENEWAL_FAILED:
		printf("UPNP_EVENT_AUTORENEWAL_FAILED\n");break;
        case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
		printf("UPNP_EVENT_SUBSCRIPTION_EXPIRED\n");break;
        case UPNP_EVENT_SUBSCRIPTION_REQUEST:
		printf("UPNP_EVENT_SUBSCRIPTION_REQUEST\n");break;
        case UPNP_CONTROL_GET_VAR_REQUEST:
		printf("UPNP_CONTROL_GET_VAR_REQUEST\n");break;
        case UPNP_CONTROL_ACTION_REQUEST:
		printf("UPNP_CONTROL_ACTION_REQUEST\n");break;
    }	

    return ret;
}





